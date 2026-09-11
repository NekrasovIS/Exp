#include "ui/ChatMessageRow.h"

#include <gtest/gtest.h>

#include <QAction>
#include <QLabel>
#include <QMenu>
#include <QPoint>
#include <QSignalSpy>

namespace devicehub {
namespace {

TEST(ChatMessageRowMarkdownTest, BodyLabelUsesMarkdownTextFormat) {
    const ChatMessage message{.author = QStringLiteral("alice"),
                               .body = QStringLiteral("**bold** and *italic*"),
                               .sentAt = QStringLiteral("2026-08-05 09:14:23.123456")};
    ChatMessageRow row(message, /*showHeader=*/true, /*isOwnMessage=*/false);

    auto* bodyLabel = row.findChild<QLabel*>(QStringLiteral("chatMessageBody"));
    ASSERT_NE(bodyLabel, nullptr);
    EXPECT_EQ(bodyLabel->textFormat(), Qt::MarkdownText);
    EXPECT_TRUE(bodyLabel->openExternalLinks());
}

// Issue #307 — highlightMentions() itself is unit-tested directly in
// MessageFormattingTest.cpp; these two only check the wiring: the
// wrapped text actually reaches the label, and editRequested() still
// emits the ORIGINAL body, not the **bold**-wrapped one (see
// ChatMessageRow::rawBody_'s own doc comment on why that distinction
// matters — re-saving an edit unchanged must not bake the wrapper in).

TEST(ChatMessageRowMarkdownTest, BodyLabelShowsAMentionWrappedInBold) {
    const ChatMessage message{.author = QStringLiteral("alice"),
                               .body = QStringLiteral("hi @bob"),
                               .sentAt = QStringLiteral("2026-08-05 09:14:23.123456")};
    ChatMessageRow row(message, /*showHeader=*/true, /*isOwnMessage=*/false);

    auto* bodyLabel = row.findChild<QLabel*>(QStringLiteral("chatMessageBody"));
    ASSERT_NE(bodyLabel, nullptr);
    EXPECT_EQ(bodyLabel->text(), QStringLiteral("hi **@bob**"));
}

TEST(ChatMessageRowMarkdownTest, EditRequestedCarriesTheUnwrappedBodyNotTheHighlightedOne) {
    const ChatMessage message{.author = QStringLiteral("alice"),
                               .body = QStringLiteral("hi @bob"),
                               .sentAt = QStringLiteral("2026-08-05 09:14:23.123456")};
    ChatMessageRow row(message, /*showHeader=*/true, /*isOwnMessage=*/true);

    auto* bubble = row.findChild<QWidget*>(QStringLiteral("chatMessageBubble"));
    ASSERT_NE(bubble, nullptr);
    emit bubble->customContextMenuRequested(QPoint(5, 5));
    auto* menu = bubble->findChild<QMenu*>(QStringLiteral("chatMessageContextMenu"));
    ASSERT_NE(menu, nullptr);
    auto* editAction = menu->findChild<QAction*>(QStringLiteral("editMessageAction"));
    ASSERT_NE(editAction, nullptr);

    QSignalSpy spy(&row, &ChatMessageRow::editRequested);
    editAction->trigger();

    ASSERT_EQ(spy.count(), 1);
    EXPECT_EQ(spy.at(0).at(1).toString(), QStringLiteral("hi @bob"));
}

}  // namespace
}  // namespace devicehub
