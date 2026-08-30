#include "ui/ChatView.h"

#include <gtest/gtest.h>

#include <QLayout>
#include <QPushButton>

namespace devicehub {
namespace {

TEST(ChatViewMessageHistoryTest, LoadOlderButtonStartsHidden) {
    ChatView view;
    view.showChannel(QStringLiteral("general"));

    ASSERT_NE(view.loadOlderButton(), nullptr);
    EXPECT_FALSE(view.loadOlderButton()->isVisible());
}

TEST(ChatViewMessageHistoryTest, SetLoadOlderVisibleTogglesTheButton) {
    ChatView view;
    view.showChannel(QStringLiteral("general"));

    view.setLoadOlderVisible(true);
    EXPECT_TRUE(view.loadOlderButton()->isVisible());

    view.setLoadOlderVisible(false);
    EXPECT_FALSE(view.loadOlderButton()->isVisible());
}

TEST(ChatViewMessageHistoryTest, ClickingLoadOlderEmitsSignal) {
    ChatView view;
    view.showChannel(QStringLiteral("general"));

    int emitCount = 0;
    QObject::connect(&view, &ChatView::loadOlderMessagesRequested, [&]() { ++emitCount; });
    view.loadOlderButton()->click();
    EXPECT_EQ(emitCount, 1);
}

TEST(ChatViewMessageHistoryTest, PrependMessagesInsertsAboveExistingContent) {
    ChatView view;
    view.showChannel(QStringLiteral("general"));

    view.appendMessage(ChatMessage{.id = 2, .author = QStringLiteral("alice"), .body = QStringLiteral("newest"),
                                    .sentAt = QStringLiteral("2026-08-05 09:05:00")});
    view.prependMessages({ChatMessage{.id = 1, .author = QStringLiteral("alice"), .body = QStringLiteral("older"),
                                       .sentAt = QStringLiteral("2026-08-05 09:00:00")}});

    QLayout* layout = view.messagesContainer()->layout();
    ASSERT_NE(layout, nullptr);
    // Two message rows, oldest first — prependMessages() inserted
    // before the message that was appended earlier, not after it.
    ASSERT_GE(layout->count(), 2);
    EXPECT_NE(layout->itemAt(0)->widget(), nullptr);
    EXPECT_NE(layout->itemAt(1)->widget(), nullptr);
    EXPECT_NE(layout->itemAt(0)->widget(), layout->itemAt(1)->widget());
}

TEST(ChatViewMessageHistoryTest, PrependingEmptyListIsANoOp) {
    ChatView view;
    view.showChannel(QStringLiteral("general"));

    QLayout* layout = view.messagesContainer()->layout();
    const int countBefore = layout->count();
    view.prependMessages({});
    EXPECT_EQ(layout->count(), countBefore);
}

}  // namespace
}  // namespace devicehub
