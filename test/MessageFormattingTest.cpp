#include "ui/MessageFormatting.h"

#include <gtest/gtest.h>

#include <QString>

namespace devicehub {
namespace {

TEST(MessageFormattingTest, WrapsASingleMentionInBold) {
    EXPECT_EQ(message_formatting::highlightMentions(QStringLiteral("hi @bob")),
              QStringLiteral("hi **@bob**"));
}

TEST(MessageFormattingTest, WrapsMultipleMentions) {
    EXPECT_EQ(message_formatting::highlightMentions(QStringLiteral("@alice and @bob, take a look")),
              QStringLiteral("**@alice** and **@bob**, take a look"));
}

TEST(MessageFormattingTest, LeavesTextWithoutMentionsUnchanged) {
    EXPECT_EQ(message_formatting::highlightMentions(QStringLiteral("no mentions here")),
              QStringLiteral("no mentions here"));
}

TEST(MessageFormattingTest, MatchesLoginsWithDigitsUnderscoresAndHyphens) {
    EXPECT_EQ(message_formatting::highlightMentions(QStringLiteral("@bob_2-test hi")),
              QStringLiteral("**@bob_2-test** hi"));
}

TEST(MessageFormattingTest, DoesNotTreatAnEmailAddressAsAMention) {
    EXPECT_EQ(message_formatting::highlightMentions(QStringLiteral("reach me at bob@example.com")),
              QStringLiteral("reach me at bob@example.com"));
}

TEST(MessageFormattingTest, MentionAtTheVeryStartOfTheStringIsStillWrapped) {
    EXPECT_EQ(message_formatting::highlightMentions(QStringLiteral("@bob hi")), QStringLiteral("**@bob** hi"));
}

TEST(MessageFormattingTest, HandlesAnEmptyString) {
    EXPECT_EQ(message_formatting::highlightMentions(QStringLiteral("")), QStringLiteral(""));
}

}  // namespace
}  // namespace devicehub
