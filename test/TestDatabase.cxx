#include "Database.hxx"
#include "Filter.hxx"
#include "Selection.hxx"
#include "net/log/Serializer.hxx"
#include "time/ClockCache.hxx"

#include <gtest/gtest.h>

#include <optional>

using std::string_view_literals::operator""sv;

static constexpr auto
MakeTimestamp(unsigned t)
{
	return Net::Log::TimePoint(Net::Log::Duration(t));
}

static const Record &
Push(Database &db, const Net::Log::Datagram &src)
{
	std::byte buffer[16384];
	size_t size = Net::Log::Serialize(buffer, src);
	return db.Emplace({buffer, size});
}

static const Record *
CheckPush(Database &db, const Net::Log::Datagram &src,
          const ClockCache<std::chrono::steady_clock> &clock)
{
	std::byte buffer[16384];
	size_t size = Net::Log::Serialize(buffer, src);
	return db.CheckEmplace({buffer, size}, clock);
}

TEST(Database, Basic)
{
	Database db(64 * 1024);
	EXPECT_TRUE(db.GetAllRecords().empty());

	Net::Log::Datagram d;
	d.timestamp = MakeTimestamp(1);
	Push(db, d);

	EXPECT_FALSE(db.GetAllRecords().empty());
	EXPECT_EQ(db.GetAllRecords().front().GetParsed().timestamp, MakeTimestamp(1));
	EXPECT_EQ(db.GetAllRecords().back().GetParsed().timestamp, MakeTimestamp(1));

	for (unsigned i = 2; i < 32768; ++i) {
		d.timestamp = MakeTimestamp(i);
		Push(db, d);

		EXPECT_EQ(db.GetAllRecords().back().GetParsed().timestamp, MakeTimestamp(i));
	}

	/* by now, the first record must have been evicted to make room
	   for newer records */
	EXPECT_NE(db.GetAllRecords().front().GetParsed().timestamp, MakeTimestamp(1));

	/* test DeleteOlderThan() */
	auto oldest = db.GetAllRecords().front().GetParsed().timestamp + Net::Log::Duration(16);
	db.DeleteOlderThan(oldest);
	EXPECT_EQ(db.GetAllRecords().front().GetParsed().timestamp, oldest);
}

TEST(Database, PerSite)
{
	Database db{64 * 1024};
	EXPECT_TRUE(db.GetAllRecords().empty());

	for (unsigned i = 1; i <= 8; ++i) {
		Push(db, {.timestamp = MakeTimestamp(i), .site = "a"});
		Push(db, {.timestamp = MakeTimestamp(i), .site = "b"});
	}

	ASSERT_FALSE(db.GetAllRecords().empty());
	EXPECT_EQ(db.GetAllRecords().front().GetParsed().timestamp, MakeTimestamp(1));
	EXPECT_EQ(db.GetAllRecords().back().GetParsed().timestamp, MakeTimestamp(8));

	for (const char *site : {"a", "b"}) {
		auto selection = db.Select({.sites={site}});

		for (unsigned i = 1; i <= 8; ++i, ++selection) {
			ASSERT_TRUE(selection);
			EXPECT_EQ(selection->GetParsed().timestamp, MakeTimestamp(i));
		}

		EXPECT_FALSE(selection);
	}

	std::optional<Selection> c = db.Select({.sites={"c"}});
	EXPECT_FALSE(*c);
	c->Rewind();
	EXPECT_FALSE(*c);

	Push(db, {.timestamp = MakeTimestamp(9), .site = "c"});
	c->Rewind();
	EXPECT_TRUE(*c);

	for (unsigned i = 10; i <= 16; ++i) {
		Push(db, {.timestamp = MakeTimestamp(i), .site = "a"});
		Push(db, {.timestamp = MakeTimestamp(i), .site = "c"});
		EXPECT_TRUE(*c);
	}

	for (unsigned i = 9; i <= 16; ++i, ++(*c)) {
		c->FixDeleted();
		db.Compress();
		ASSERT_TRUE(*c);
		EXPECT_EQ((*c)->GetParsed().timestamp, MakeTimestamp(i));
	}
	EXPECT_FALSE(*c);

	c->Rewind();
	ASSERT_TRUE(*c);
	EXPECT_EQ((*c)->GetParsed().timestamp, MakeTimestamp(9));

	EXPECT_TRUE(db.Select({.sites={"a"}}));
	EXPECT_TRUE(db.Select({.sites={"b"}}));

	db.DeleteOlderThan(MakeTimestamp(10));

	ASSERT_TRUE(db.Select({.sites={"a"}}));
	ASSERT_EQ(db.Select({.sites={"a"}})->GetParsed().timestamp, MakeTimestamp(10));
	EXPECT_FALSE(db.Select({.sites={"b"}}));

	c->FixDeleted();
	ASSERT_TRUE(*c);
	EXPECT_EQ((*c)->GetParsed().timestamp, MakeTimestamp(10));

	db.DeleteOlderThan(MakeTimestamp(11));

	ASSERT_TRUE(db.Select({.sites={"a"}}));
	ASSERT_EQ(db.Select({.sites={"a"}})->GetParsed().timestamp, MakeTimestamp(11));
	EXPECT_FALSE(db.Select({.sites={"b"}}));

	c->FixDeleted();
	ASSERT_TRUE(*c);
	EXPECT_EQ((*c)->GetParsed().timestamp, MakeTimestamp(11));

	Push(db, {.timestamp = MakeTimestamp(17), .site = "c"});
	Push(db, {.timestamp = MakeTimestamp(18), .site = "c"});
	Push(db, {.timestamp = MakeTimestamp(19), .site = "a"});

	db.Compress();

	for (unsigned i = 11; i <= 18; ++i, ++(*c)) {
		ASSERT_TRUE(*c);
		EXPECT_EQ((*c)->GetParsed().timestamp, MakeTimestamp(i));
	}
	EXPECT_FALSE(*c);

	c->Rewind();
	ASSERT_TRUE(*c);
	EXPECT_EQ((*c)->GetParsed().timestamp, MakeTimestamp(11));

	db.DeleteOlderThan(MakeTimestamp(19));
	c->FixDeleted();
	EXPECT_FALSE(*c);

	c.reset();

	db.Compress();

	ASSERT_TRUE(db.Select({.sites={"a"}}));
	ASSERT_EQ(db.Select({.sites={"a"}})->GetParsed().timestamp, MakeTimestamp(19));
	EXPECT_FALSE(db.Select({.sites={"b"}}));
	EXPECT_FALSE(db.Select({.sites={"c"}}));
}

static bool
IsRateLimited(Database &db, const Net::Log::Datagram &src,
              const ClockCache<std::chrono::steady_clock> &clock,
              unsigned n)
{
	for (unsigned i = 0; i < n; ++i)
		if (!CheckPush(db, src, clock))
			return true;

	return false;
}

TEST(Database, PerSiteRateLimit)
{
	Database db(256 * 1024, 10);
	EXPECT_TRUE(db.GetAllRecords().empty());

	const std::chrono::steady_clock::time_point zero;
	const std::chrono::steady_clock::time_point start = zero + std::chrono::hours(42);
	ClockCache<std::chrono::steady_clock> clock;

	Net::Log::Datagram d;
	d.timestamp = MakeTimestamp(1);
	Push(db, d);

	clock.Mock(start);

	/* no site: not rate limited */
	EXPECT_FALSE(IsRateLimited(db, d, clock, 256));

	/* not a "message" datagram: not rate limited */
	d.site = "foo";
	EXPECT_FALSE(IsRateLimited(db, d, clock, 256));

	/* this should be rate limited */
	d.type = Net::Log::Type::HTTP_ERROR;
	EXPECT_FALSE(IsRateLimited(db, d, clock, 1));
	EXPECT_TRUE(IsRateLimited(db, d, clock, 256));

	/* different site, new rate limit state */
	d.site = "bar";
	EXPECT_FALSE(IsRateLimited(db, d, clock, 1));
	EXPECT_TRUE(IsRateLimited(db, d, clock, 256));

	/* back to first site, still discarding at this time */
	d.site = "foo";
	EXPECT_TRUE(IsRateLimited(db, d, clock, 1));

	/* fast-forward one second: 10 more messages allowed */
	clock.Mock(start + std::chrono::seconds(1));
	EXPECT_FALSE(IsRateLimited(db, d, clock, 10));
	EXPECT_TRUE(IsRateLimited(db, d, clock, 1));

	/* fast-forward another second: 10 more messages allowed */
	clock.Mock(start + std::chrono::seconds(2));
	EXPECT_FALSE(IsRateLimited(db, d, clock, 10));
	EXPECT_TRUE(IsRateLimited(db, d, clock, 1));

	/* fast-forward half a second: 5 more messages allowed */
	clock.Mock(start + std::chrono::milliseconds(2500));
	EXPECT_FALSE(IsRateLimited(db, d, clock, 5));
	EXPECT_TRUE(IsRateLimited(db, d, clock, 1));

	/* fast-forward half a second: 5 more messages allowed */
	clock.Mock(start + std::chrono::seconds(3));
	EXPECT_FALSE(IsRateLimited(db, d, clock, 5));
	EXPECT_TRUE(IsRateLimited(db, d, clock, 1));

	/* no site, no rate limit */
	d.site = nullptr;
	EXPECT_FALSE(IsRateLimited(db, d, clock, 256));
}
