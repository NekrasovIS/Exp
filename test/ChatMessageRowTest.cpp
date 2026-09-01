#include "ui/ChatMessageRow.h"

#include <gtest/gtest.h>

#include <QLabel>
#include <QPushButton>
#include <QSignalSpy>

namespace devicehub {
namespace {

ChatMessage sampleMessage() {
    return ChatMessage{.author = "alice", .body = "hello", .sentAt = "2026-08-05 09:00:00"};
}

ChatMessage sampleMessageWithAttachment() {
    return ChatMessage{.author = "alice",
                        .body = "see attached",
                        .sentAt = "2026-08-05 09:00:00",
                        .attachmentId = 42,
                        .attachmentFilename = "report.pdf"};
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

TEST(ChatMessageRowTest, MessageWithoutAttachmentHasNoDownloadButton) {
    ChatMessageRow row(sampleMessage(), /*showHeader=*/true, /*isOwnMessage=*/false);

    EXPECT_EQ(row.findChild<QPushButton*>("downloadAttachmentButton"), nullptr);
}

TEST(ChatMessageRowTest, MessageWithAttachmentShowsDownloadButtonAndEmitsOnClick) {
    ChatMessageRow row(sampleMessageWithAttachment(), /*showHeader=*/true, /*isOwnMessage=*/false);

    auto* downloadButton = row.findChild<QPushButton*>("downloadAttachmentButton");
    ASSERT_NE(downloadButton, nullptr);
    EXPECT_TRUE(downloadButton->text().contains("report.pdf"));

    QSignalSpy spy(&row, &ChatMessageRow::downloadRequested);
    downloadButton->click();
    ASSERT_EQ(spy.count(), 1);
    EXPECT_EQ(spy.at(0).at(0).toLongLong(), 42);
    EXPECT_EQ(spy.at(0).at(1).toString(), QStringLiteral("report.pdf"));
}

TEST(ChatMessageRowTest, OwnMessageEditButtonEmitsEditRequestedWithIdAndCurrentBody) {
    ChatMessage message = sampleMessage();
    message.id = 7;
    ChatMessageRow row(message, /*showHeader=*/true, /*isOwnMessage=*/true);

    auto* editButton = row.findChild<QPushButton*>("editMessageButton");
    ASSERT_NE(editButton, nullptr);

    QSignalSpy spy(&row, &ChatMessageRow::editRequested);
    editButton->click();
    ASSERT_EQ(spy.count(), 1);
    EXPECT_EQ(spy.at(0).at(0).toLongLong(), 7);
    EXPECT_EQ(spy.at(0).at(1).toString(), QStringLiteral("hello"));
}

TEST(ChatMessageRowTest, OwnMessageDeleteButtonEmitsDeleteRequestedWithId) {
    ChatMessage message = sampleMessage();
    message.id = 7;
    ChatMessageRow row(message, /*showHeader=*/true, /*isOwnMessage=*/true);

    auto* deleteButton = row.findChild<QPushButton*>("deleteMessageButton");
    ASSERT_NE(deleteButton, nullptr);

    QSignalSpy spy(&row, &ChatMessageRow::deleteRequested);
    deleteButton->click();
    ASSERT_EQ(spy.count(), 1);
    EXPECT_EQ(spy.at(0).at(0).toLongLong(), 7);
}

TEST(ChatMessageRowTest, NonOwnMessageHasNoEditOrDeleteButtons) {
    ChatMessageRow row(sampleMessage(), /*showHeader=*/true, /*isOwnMessage=*/false);

    EXPECT_EQ(row.findChild<QPushButton*>("editMessageButton"), nullptr);
    EXPECT_EQ(row.findChild<QPushButton*>("deleteMessageButton"), nullptr);
}

}  // namespace
}  // namespace devicehub
