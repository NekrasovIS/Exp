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
    const ChatMessage previous{.author = QStringLiteral("maria"), .body = QStringLiteral("hi"), .sentAt = QStringLiteral("2026-08-05 09:00:00")};
    const ChatMessage current{.author = QStringLiteral("maria"), .body = QStringLiteral("there"), .sentAt = QStringLiteral("2026-08-05 09:04:59")};
    EXPECT_TRUE(chat_message_grouping::shouldGroupWithPrevious(previous, current));
}

TEST(ChatMessageGroupingTest, DoesNotGroupDifferentAuthors) {
    const ChatMessage previous{.author = QStringLiteral("maria"), .body = QStringLiteral("hi"), .sentAt = QStringLiteral("2026-08-05 09:00:00")};
    const ChatMessage current{.author = QStringLiteral("ilya"), .body = QStringLiteral("hey"), .sentAt = QStringLiteral("2026-08-05 09:00:01")};
    EXPECT_FALSE(chat_message_grouping::shouldGroupWithPrevious(previous, current));
}

TEST(ChatMessageGroupingTest, DoesNotGroupAcrossALongGap) {
    const ChatMessage previous{.author = QStringLiteral("maria"), .body = QStringLiteral("hi"), .sentAt = QStringLiteral("2026-08-05 09:00:00")};
    const ChatMessage current{.author = QStringLiteral("maria"), .body = QStringLiteral("still there?"), .sentAt = QStringLiteral("2026-08-05 09:06:01")};
    EXPECT_FALSE(chat_message_grouping::shouldGroupWithPrevious(previous, current));
}

TEST(ChatMessageGroupingTest, DoesNotGroupWhenATimestampFailsToParse) {
    const ChatMessage previous{.author = QStringLiteral("maria"), .body = QStringLiteral("hi"), .sentAt = QStringLiteral("garbage")};
    const ChatMessage current{.author = QStringLiteral("maria"), .body = QStringLiteral("there"), .sentAt = QStringLiteral("2026-08-05 09:00:01")};
    EXPECT_FALSE(chat_message_grouping::shouldGroupWithPrevious(previous, current));
}

TEST(ChatMessageGroupingTest, SameCalendarDayIsNotADifferentDay) {
    const ChatMessage previous{.sentAt = QStringLiteral("2026-08-05 09:00:00")};
    const ChatMessage current{.sentAt = QStringLiteral("2026-08-05 23:59:59")};
    EXPECT_FALSE(chat_message_grouping::isDifferentCalendarDay(previous, current));
}

TEST(ChatMessageGroupingTest, CrossingMidnightIsADifferentDay) {
    const ChatMessage previous{.sentAt = QStringLiteral("2026-08-05 23:59:59")};
    const ChatMessage current{.sentAt = QStringLiteral("2026-08-06 00:00:01")};
    EXPECT_TRUE(chat_message_grouping::isDifferentCalendarDay(previous, current));
}

TEST(ChatMessageGroupingTest, UnparseableTimestampCountsAsADifferentDay) {
    // issue #188: в отличие от shouldGroupWithPrevious(), здесь "не
    // уверены" означает true, а не false — лучше показать лишний
    // разделитель даты, чем незаметно скрыть настоящую границу дня.
    const ChatMessage previous{.sentAt = QStringLiteral("garbage")};
    const ChatMessage current{.sentAt = QStringLiteral("2026-08-05 09:00:01")};
    EXPECT_TRUE(chat_message_grouping::isDifferentCalendarDay(previous, current));
}

}  // namespace
}  // namespace devicehub
