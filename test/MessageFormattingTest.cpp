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

// Issue #396/#398 — link previews.

TEST(MessageFormattingTest, FindFirstUrlReturnsEmptyStringWhenThereIsNoUrl) {
    EXPECT_EQ(message_formatting::findFirstUrl(QStringLiteral("just some plain text")), QString());
}

TEST(MessageFormattingTest, FindFirstUrlFindsAnHttpsUrlEmbeddedInASentence) {
    EXPECT_EQ(message_formatting::findFirstUrl(QStringLiteral("check this out: https://example.test/article")),
              QStringLiteral("https://example.test/article"));
}

TEST(MessageFormattingTest, FindFirstUrlFindsAnHttpUrl) {
    EXPECT_EQ(message_formatting::findFirstUrl(QStringLiteral("http://example.test/")),
              QStringLiteral("http://example.test/"));
}

TEST(MessageFormattingTest, FindFirstUrlReturnsOnlyTheFirstUrlWhenThereAreSeveral) {
    EXPECT_EQ(message_formatting::findFirstUrl(QStringLiteral("https://a.test/ and https://b.test/")),
              QStringLiteral("https://a.test/"));
}

TEST(MessageFormattingTest, FindFirstUrlStopsAtWhitespace) {
    EXPECT_EQ(message_formatting::findFirstUrl(QStringLiteral("https://example.test/path first then more text")),
              QStringLiteral("https://example.test/path"));
}

TEST(MessageFormattingTest, FindFirstUrlDoesNotMatchABareDomainWithoutAScheme) {
    EXPECT_EQ(message_formatting::findFirstUrl(QStringLiteral("visit example.test today")), QString());
}

}  // namespace
}  // namespace devicehub
