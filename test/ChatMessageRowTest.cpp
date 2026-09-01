#include "ui/ChatMessageRow.h"

#include <gtest/gtest.h>

#include <QAction>
#include <QLabel>
#include <QMenu>
#include <QPoint>
#include <QPushButton>
#include <QSignalSpy>
#include <QWidget>

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

TEST(ChatMessageRowTest, NonOwnMessageHasNoContextMenu) {
    ChatMessageRow row(sampleMessage(), /*showHeader=*/true, /*isOwnMessage=*/false);

    auto* bubble = row.findChild<QWidget*>(QStringLiteral("chatMessageBubble"));
    ASSERT_NE(bubble, nullptr);
    EXPECT_EQ(bubble->contextMenuPolicy(), Qt::DefaultContextMenu);
}

TEST(ChatMessageRowTest, OwnMessageContextMenuEditActionEmitsEditRequestedWithCurrentBody) {
    ChatMessageRow row(sampleMessage(), /*showHeader=*/true, /*isOwnMessage=*/true);

    auto* bubble = row.findChild<QWidget*>(QStringLiteral("chatMessageBubble"));
    ASSERT_NE(bubble, nullptr);
    EXPECT_EQ(bubble->contextMenuPolicy(), Qt::CustomContextMenu);

    // popup() (used by the handler) doesn't block, so the resulting menu
    // exists as a child object right after the signal returns — no need
    // to drive a real right-click or a modal event loop to test this.
    emit bubble->customContextMenuRequested(QPoint(5, 5));
    auto* menu = bubble->findChild<QMenu*>(QStringLiteral("chatMessageContextMenu"));
    ASSERT_NE(menu, nullptr);
    auto* editAction = menu->findChild<QAction*>(QStringLiteral("editMessageAction"));
    ASSERT_NE(editAction, nullptr);

    QSignalSpy spy(&row, &ChatMessageRow::editRequested);
    editAction->trigger();
    ASSERT_EQ(spy.count(), 1);
    EXPECT_EQ(spy.at(0).at(0).toLongLong(), row.messageId());
    EXPECT_EQ(spy.at(0).at(1).toString(), QStringLiteral("hello"));
}

TEST(ChatMessageRowTest, OwnMessageContextMenuDeleteActionEmitsDeleteRequested) {
    ChatMessageRow row(sampleMessage(), /*showHeader=*/true, /*isOwnMessage=*/true);

    auto* bubble = row.findChild<QWidget*>(QStringLiteral("chatMessageBubble"));
    ASSERT_NE(bubble, nullptr);

    emit bubble->customContextMenuRequested(QPoint(5, 5));
    auto* menu = bubble->findChild<QMenu*>(QStringLiteral("chatMessageContextMenu"));
    ASSERT_NE(menu, nullptr);
    auto* deleteAction = menu->findChild<QAction*>(QStringLiteral("deleteMessageAction"));
    ASSERT_NE(deleteAction, nullptr);

    QSignalSpy spy(&row, &ChatMessageRow::deleteRequested);
    deleteAction->trigger();
    ASSERT_EQ(spy.count(), 1);
    EXPECT_EQ(spy.at(0).at(0).toLongLong(), row.messageId());
}

}  // namespace
}  // namespace devicehub
