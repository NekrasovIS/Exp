#include "ui/ChatMessageRow.h"

#include <gtest/gtest.h>

#include <QLabel>

namespace devicehub {
namespace {

TEST(ChatMessageRowMarkdownTest, BodyLabelUsesMarkdownTextFormat) {
    const ChatMessage message{QStringLiteral("alice"), QStringLiteral("**bold** and *italic*"),
                               QStringLiteral("2026-08-05 09:14:23.123456")};
    ChatMessageRow row(message, /*showHeader=*/true, /*isOwnMessage=*/false);

    auto* bodyLabel = row.findChild<QLabel*>(QStringLiteral("chatMessageBody"));
    ASSERT_NE(bodyLabel, nullptr);
    EXPECT_EQ(bodyLabel->textFormat(), Qt::MarkdownText);
    EXPECT_TRUE(bodyLabel->openExternalLinks());
}

}  // namespace
}  // namespace devicehub
