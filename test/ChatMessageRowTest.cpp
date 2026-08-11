#include "ui/ChatMessageRow.h"

#include <gtest/gtest.h>

#include <QLabel>

namespace devicehub {
namespace {

ChatMessage sampleMessage() {
    return ChatMessage{.author = "alice", .body = "hello", .sentAt = "2026-08-05 09:00:00"};
}

TEST(ChatMessageRowTest, NonOwnMessageWithHeaderHasAvatarAuthorAndTimeLabels) {
    ChatMessageRow row(sampleMessage(), /*showHeader=*/true, /*isOwnMessage=*/false);

    // Avatar (unnamed QLabel) + author (property "chatAuthor") + time
    // ("mutedDescription") + body = 4 QLabels; own messages never get
    // an author label (see NonOwnMessageWithHeader below), which is
    // what distinguishes this count from the own-message case.
    EXPECT_EQ(row.findChildren<QLabel*>().size(), 4);
    int authorLabelCount = 0;
    for (const QLabel* label : row.findChildren<QLabel*>()) {
        if (label->property("chatAuthor").toBool()) {
            ++authorLabelCount;
        }
    }
    EXPECT_EQ(authorLabelCount, 1);
}

TEST(ChatMessageRowTest, OwnMessageWithHeaderHasNoAuthorLabel) {
    ChatMessageRow row(sampleMessage(), /*showHeader=*/true, /*isOwnMessage=*/true);

    // Time + body only — own messages skip the avatar and author label.
    EXPECT_EQ(row.findChildren<QLabel*>().size(), 2);
    for (const QLabel* label : row.findChildren<QLabel*>()) {
        EXPECT_FALSE(label->property("chatAuthor").toBool());
    }
}

TEST(ChatMessageRowTest, GroupedMessageWithoutHeaderHasOnlyBodyLabel) {
    ChatMessageRow row(sampleMessage(), /*showHeader=*/false, /*isOwnMessage=*/false);

    EXPECT_EQ(row.findChildren<QLabel*>().size(), 1);
}

}  // namespace
}  // namespace devicehub
