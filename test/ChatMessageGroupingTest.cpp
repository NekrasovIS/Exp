#include "ui/ChatMessageGrouping.h"

#include <gtest/gtest.h>

namespace devicehub {
namespace {

TEST(ChatMessageGroupingTest, ParsesSpaceSeparatedPostgresTimestamp) {
    const QDateTime parsed = chat_message_grouping::parseSentAt(QStringLiteral("2026-08-05 09:14:23.123456"));
    ASSERT_TRUE(parsed.isValid());
    EXPECT_EQ(parsed.date(), QDate(2026, 8, 5));
    EXPECT_EQ(parsed.time().hour(), 9);
    EXPECT_EQ(parsed.time().minute(), 14);
}

TEST(ChatMessageGroupingTest, ParsesIsoTimestampToo) {
    const QDateTime parsed = chat_message_grouping::parseSentAt(QStringLiteral("2026-08-05T09:14:23.123"));
    EXPECT_TRUE(parsed.isValid());
}

TEST(ChatMessageGroupingTest, RejectsGarbage) {
    EXPECT_FALSE(chat_message_grouping::parseSentAt(QStringLiteral("not a timestamp")).isValid());
}

TEST(ChatMessageGroupingTest, GroupsSameAuthorWithinFiveMinutes) {
    const ChatMessage previous{QStringLiteral("maria"), QStringLiteral("hi"), QStringLiteral("2026-08-05 09:00:00")};
    const ChatMessage current{QStringLiteral("maria"), QStringLiteral("there"), QStringLiteral("2026-08-05 09:04:59")};
    EXPECT_TRUE(chat_message_grouping::shouldGroupWithPrevious(previous, current));
}

TEST(ChatMessageGroupingTest, DoesNotGroupDifferentAuthors) {
    const ChatMessage previous{QStringLiteral("maria"), QStringLiteral("hi"), QStringLiteral("2026-08-05 09:00:00")};
    const ChatMessage current{QStringLiteral("ilya"), QStringLiteral("hey"), QStringLiteral("2026-08-05 09:00:01")};
    EXPECT_FALSE(chat_message_grouping::shouldGroupWithPrevious(previous, current));
}

TEST(ChatMessageGroupingTest, DoesNotGroupAcrossALongGap) {
    const ChatMessage previous{QStringLiteral("maria"), QStringLiteral("hi"), QStringLiteral("2026-08-05 09:00:00")};
    const ChatMessage current{QStringLiteral("maria"), QStringLiteral("still there?"), QStringLiteral("2026-08-05 09:06:01")};
    EXPECT_FALSE(chat_message_grouping::shouldGroupWithPrevious(previous, current));
}

TEST(ChatMessageGroupingTest, DoesNotGroupWhenATimestampFailsToParse) {
    const ChatMessage previous{QStringLiteral("maria"), QStringLiteral("hi"), QStringLiteral("garbage")};
    const ChatMessage current{QStringLiteral("maria"), QStringLiteral("there"), QStringLiteral("2026-08-05 09:00:01")};
    EXPECT_FALSE(chat_message_grouping::shouldGroupWithPrevious(previous, current));
}

}  // namespace
}  // namespace devicehub
