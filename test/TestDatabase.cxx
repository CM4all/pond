#include "Database.hxx"
#include "Filter.hxx"
#include "Selection.hxx"
#include "AppendListener.hxx"
#include "net/log/Serializer.hxx"
#include "time/ClockCache.hxx"

#include <gtest/gtest.h>

#include <optional>
#include <vector>

using std::string_view_literals::operator""sv;

static constexpr auto
MakeTimestamp(unsigned t)
{
	/* start at this offset to avoid integer underflows */
	constexpr Net::Log::Duration offset = std::chrono::hours{24};

	return Net::Log::TimePoint(offset + Net::Log::Duration(t));
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

	/* test SelectLast() */
	{
		Filter filter;
		auto selection = db.SelectLast(filter);
		ASSERT_EQ(selection.Update(), Selection::UpdateResult::READY);
		EXPECT_EQ(selection->GetParsed().timestamp, MakeTimestamp(32767));

		++selection;
		ASSERT_EQ(selection.Update(), Selection::UpdateResult::END);

		filter.timestamp.until = MakeTimestamp(32750);
		selection = db.SelectLast(filter);
		ASSERT_EQ(selection.Update(), Selection::UpdateResult::READY);
		EXPECT_EQ(selection->GetParsed().timestamp, MakeTimestamp(32750));

		++selection;
		ASSERT_EQ(selection.Update(), Selection::UpdateResult::END);
	}

	/* test DeleteOlderThan() */
	auto oldest = db.GetAllRecords().front().GetParsed().timestamp + Net::Log::Duration(16);
	db.DeleteOlderThan(oldest);
	EXPECT_EQ(db.GetAllRecords().front().GetParsed().timestamp, oldest);
}

TEST(Database, PerSite)
{
	Database db{64 * 1024};
	EXPECT_TRUE(db.GetAllRecords().empty());

	EXPECT_FALSE(db.GetFirstSite());

	for (unsigned i = 1; i <= 8; ++i) {
		Push(db, {.timestamp = MakeTimestamp(i), .site = "a"});
		Push(db, {.timestamp = MakeTimestamp(i), .site = "b"});
	}

	{
		auto i = db.GetFirstSite();
		ASSERT_TRUE(i);

		auto a = db.Select(i, {});
		ASSERT_EQ(a.Update(), Selection::UpdateResult::READY);
		EXPECT_EQ(a->GetParsed().timestamp, MakeTimestamp(1));
		EXPECT_STREQ(a->GetParsed().site, "a");

		i = db.GetNextSite(i);
		ASSERT_TRUE(i);

		auto b = db.Select(i, {});
		ASSERT_EQ(b.Update(), Selection::UpdateResult::READY);
		EXPECT_EQ(b->GetParsed().timestamp, MakeTimestamp(1));
		EXPECT_STREQ(b->GetParsed().site, "b");

		i = db.GetNextSite(i);
		ASSERT_FALSE(i);
	}

	ASSERT_FALSE(db.GetAllRecords().empty());
	EXPECT_EQ(db.GetAllRecords().front().GetParsed().timestamp, MakeTimestamp(1));
	EXPECT_EQ(db.GetAllRecords().back().GetParsed().timestamp, MakeTimestamp(8));

	for (const char *site : {"a", "b"}) {
		auto selection = db.Select({.sites={site}});

		for (unsigned i = 1; i <= 8; ++i, ++selection) {
			ASSERT_EQ(selection.Update(), Selection::UpdateResult::READY);
			EXPECT_EQ(selection->GetParsed().timestamp, MakeTimestamp(i));
		}

		ASSERT_EQ(selection.Update(), Selection::UpdateResult::END);

		selection = db.SelectLast({.sites={site}});
		ASSERT_EQ(selection.Update(), Selection::UpdateResult::READY);
		EXPECT_EQ(selection->GetParsed().timestamp, MakeTimestamp(8));
		++selection;
		ASSERT_EQ(selection.Update(), Selection::UpdateResult::END);
	}

	std::optional<Selection> c = db.Select({.sites={"c"}});
	ASSERT_EQ(c->Update(), Selection::UpdateResult::END);
	c->Rewind();
	ASSERT_EQ(c->Update(), Selection::UpdateResult::END);

	Push(db, {.timestamp = MakeTimestamp(9), .site = "c"});
	c->Rewind();
	ASSERT_EQ(c->Update(), Selection::UpdateResult::READY);

	for (unsigned i = 10; i <= 16; ++i) {
		Push(db, {.timestamp = MakeTimestamp(i), .site = "a"});
		Push(db, {.timestamp = MakeTimestamp(i), .site = "c"});
		ASSERT_EQ(c->Update(), Selection::UpdateResult::READY);
	}

	{
		auto i = db.GetFirstSite();
		ASSERT_TRUE(i);

		auto a = db.Select(i, {});
		ASSERT_EQ(a.Update(), Selection::UpdateResult::READY);
		EXPECT_EQ(a->GetParsed().timestamp, MakeTimestamp(1));
		EXPECT_STREQ(a->GetParsed().site, "a");

		i = db.GetNextSite(i);

		auto b = db.Select(i, {});
		ASSERT_EQ(b.Update(), Selection::UpdateResult::READY);
		EXPECT_EQ(b->GetParsed().timestamp, MakeTimestamp(1));
		EXPECT_STREQ(b->GetParsed().site, "b");

		i = db.GetNextSite(i);
		ASSERT_TRUE(i);

		auto cc = db.Select(i, {});
		ASSERT_EQ(cc.Update(), Selection::UpdateResult::READY);
		EXPECT_EQ(cc->GetParsed().timestamp, MakeTimestamp(9));
		EXPECT_STREQ(cc->GetParsed().site, "c");

		i = db.GetNextSite(i);
		ASSERT_FALSE(i);
	}

	for (unsigned i = 9; i <= 16; ++i, ++(*c)) {
		c->FixDeleted();
		db.Compress();
		ASSERT_EQ(c->Update(), Selection::UpdateResult::READY);
		EXPECT_EQ((*c)->GetParsed().timestamp, MakeTimestamp(i));
	}
	ASSERT_EQ(c->Update(), Selection::UpdateResult::END);

	c->Rewind();
	ASSERT_EQ(c->Update(), Selection::UpdateResult::READY);
	EXPECT_EQ((*c)->GetParsed().timestamp, MakeTimestamp(9));

	EXPECT_EQ(db.Select({.sites={"a"}}).Update(), Selection::UpdateResult::READY);
	EXPECT_EQ(db.Select({.sites={"b"}}).Update(), Selection::UpdateResult::READY);

	db.DeleteOlderThan(MakeTimestamp(10));

	{
		auto a = db.Select({.sites={"a"}});
		ASSERT_EQ(a.Update(), Selection::UpdateResult::READY);
		ASSERT_EQ(a->GetParsed().timestamp, MakeTimestamp(10));
	}

	EXPECT_EQ(db.Select({.sites={"b"}}).Update(), Selection::UpdateResult::END);

	c->FixDeleted();
	ASSERT_EQ(c->Update(), Selection::UpdateResult::READY);
	EXPECT_EQ((*c)->GetParsed().timestamp, MakeTimestamp(10));

	db.DeleteOlderThan(MakeTimestamp(11));

	{
		auto i = db.GetFirstSite();
		ASSERT_TRUE(i);

		auto a = db.Select(i, {});
		ASSERT_EQ(a.Update(), Selection::UpdateResult::READY);
		EXPECT_EQ(a->GetParsed().timestamp, MakeTimestamp(11));
		EXPECT_STREQ(a->GetParsed().site, "a");

		i = db.GetNextSite(i);
		ASSERT_TRUE(i);

		auto cc = db.Select(i, {});
		ASSERT_EQ(cc.Update(), Selection::UpdateResult::READY);
		EXPECT_EQ(cc->GetParsed().timestamp, MakeTimestamp(11));
		EXPECT_STREQ(cc->GetParsed().site, "c");

		i = db.GetNextSite(i);
		ASSERT_FALSE(i);
	}

	{
		auto a = db.Select({.sites={"a"}});
		ASSERT_EQ(a.Update(), Selection::UpdateResult::READY);
		ASSERT_EQ(a->GetParsed().timestamp, MakeTimestamp(11));
	}

	EXPECT_EQ(db.Select({.sites={"b"}}).Update(), Selection::UpdateResult::END);

	c->FixDeleted();
	ASSERT_EQ(c->Update(), Selection::UpdateResult::READY);
	EXPECT_EQ((*c)->GetParsed().timestamp, MakeTimestamp(11));

	Push(db, {.timestamp = MakeTimestamp(17), .site = "c"});
	Push(db, {.timestamp = MakeTimestamp(18), .site = "c"});
	Push(db, {.timestamp = MakeTimestamp(19), .site = "a"});

	db.Compress();

	for (unsigned i = 11; i <= 18; ++i, ++(*c)) {
		ASSERT_EQ(c->Update(), Selection::UpdateResult::READY);
		EXPECT_EQ((*c)->GetParsed().timestamp, MakeTimestamp(i));
	}

	ASSERT_EQ(c->Update(), Selection::UpdateResult::END);

	c->Rewind();
	ASSERT_EQ(c->Update(), Selection::UpdateResult::READY);
	EXPECT_EQ((*c)->GetParsed().timestamp, MakeTimestamp(11));

	db.DeleteOlderThan(MakeTimestamp(19));
	c->FixDeleted();
	ASSERT_EQ(c->Update(), Selection::UpdateResult::END);

	{
		auto i = db.GetFirstSite();
		ASSERT_TRUE(i);

		auto a = db.Select(i, {});
		ASSERT_EQ(a.Update(), Selection::UpdateResult::READY);
		EXPECT_EQ(a->GetParsed().timestamp, MakeTimestamp(19));
		EXPECT_STREQ(a->GetParsed().site, "a");

		i = db.GetNextSite(i);
		ASSERT_TRUE(i);

		auto cc = db.Select(i, {});
		ASSERT_EQ(cc.Update(), Selection::UpdateResult::END);

		i = db.GetNextSite(i);
		ASSERT_FALSE(i);
	}

	c.reset();

	db.Compress();

	{
		auto a = db.Select({.sites={"a"}});
		ASSERT_EQ(a.Update(), Selection::UpdateResult::READY);
		ASSERT_EQ(a->GetParsed().timestamp, MakeTimestamp(19));
	}

	EXPECT_EQ(db.Select({.sites={"c"}}).Update(), Selection::UpdateResult::END);
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

struct TestAppendListener : public AppendListener {
	std::vector<const Record *> records;

	bool OnAppend(const Record &record) noexcept override {
		records.push_back(&record);
		return true; // Keep listener active
	}
};

TEST(Database, AppendListener)
{
	Database db(64 * 1024);
	TestAppendListener listener;

	// Register listener with site filter
	Filter filter;
	filter.sites.insert("test_site");
	auto selection = db.Follow(filter, listener);

	EXPECT_TRUE(listener.records.empty());

	// Add records with different site values
	Push(db, {.timestamp = MakeTimestamp(1), .site = "test_site"});
	Push(db, {.timestamp = MakeTimestamp(2), .site = "other_site"});
	Push(db, {.timestamp = MakeTimestamp(3), .site = "test_site"});
	Push(db, {.timestamp = MakeTimestamp(4), .site = "another_site"});

	// Verify AppendListener was invoked only for matching site
	EXPECT_EQ(listener.records.size(), 2u);
	EXPECT_EQ(listener.records[0]->GetParsed().timestamp, MakeTimestamp(1));
	EXPECT_STREQ(listener.records[0]->GetParsed().site, "test_site");
	EXPECT_EQ(listener.records[1]->GetParsed().timestamp, MakeTimestamp(3));
	EXPECT_STREQ(listener.records[1]->GetParsed().site, "test_site");

	// Clear the vector
	listener.records.clear();

	// Clear database and add more records
	db.Clear();

	Push(db, {.timestamp = MakeTimestamp(10), .site = "test_site"});
	Push(db, {.timestamp = MakeTimestamp(11), .site = "different_site"});
	Push(db, {.timestamp = MakeTimestamp(12), .site = "test_site"});

	// Verify AppendListener was invoked again for matching site
	EXPECT_EQ(listener.records.size(), 2u);
	EXPECT_EQ(listener.records[0]->GetParsed().timestamp, MakeTimestamp(10));
	EXPECT_STREQ(listener.records[0]->GetParsed().site, "test_site");
	EXPECT_EQ(listener.records[1]->GetParsed().timestamp, MakeTimestamp(12));
	EXPECT_STREQ(listener.records[1]->GetParsed().site, "test_site");

	// Clear the vector again
	listener.records.clear();
}

TEST(Database, MarkRestore)
{
	Database db(64 * 1024);

	// Add several records with different sites
	Push(db, {.timestamp = MakeTimestamp(1), .site = "site_a"});
	Push(db, {.timestamp = MakeTimestamp(2), .site = "site_b"});
	Push(db, {.timestamp = MakeTimestamp(3), .site = "site_a"});
	Push(db, {.timestamp = MakeTimestamp(4), .site = "site_a"});
	Push(db, {.timestamp = MakeTimestamp(5), .site = "site_b"});

	// Create a selection for site_a records
	Filter filter;
	filter.sites.insert("site_a");
	auto selection = db.Select(filter);

	// Collect markers and expected timestamps as we iterate
	std::vector<Selection::Marker> markers;
	std::vector<unsigned> expected_timestamps;

	// Iterate through selection, creating markers at each step
	for (unsigned expected_ts : {1, 3, 4}) {
		ASSERT_EQ(selection.Update(), Selection::UpdateResult::READY);
		EXPECT_EQ(selection->GetParsed().timestamp, MakeTimestamp(expected_ts));
		EXPECT_STREQ(selection->GetParsed().site, "site_a");

		// Mark the current position
		markers.push_back(selection.Mark());
		expected_timestamps.push_back(expected_ts);

		++selection;
	}

	// Should be at end now
	ASSERT_EQ(selection.Update(), Selection::UpdateResult::END);

	// Restore each marker in reverse order and verify records
	for (int i = markers.size() - 1; i >= 0; --i) {
		selection.Restore(markers[i]);
		ASSERT_EQ(selection.Update(), Selection::UpdateResult::READY);
		EXPECT_EQ(selection->GetParsed().timestamp, MakeTimestamp(expected_timestamps[i]));
		EXPECT_STREQ(selection->GetParsed().site, "site_a");
	}
}
