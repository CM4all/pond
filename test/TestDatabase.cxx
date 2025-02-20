#include "Database.hxx"
#include "net/log/Serializer.hxx"
#include "time/ClockCache.hxx"

#include <gtest/gtest.h>

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
