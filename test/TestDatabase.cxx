#include "Database.hxx"
#include "net/log/Serializer.hxx"

#include <gtest/gtest.h>

static constexpr auto
MakeTimestamp(unsigned t)
{
    return Net::Log::TimePoint(Net::Log::Duration(t));
}

static const Record &
Push(Database &db, const Net::Log::Datagram &src)
{
    uint8_t buffer[16384];
    size_t size = Net::Log::Serialize(buffer, sizeof(buffer), src);
    return db.Emplace({buffer, size});
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
