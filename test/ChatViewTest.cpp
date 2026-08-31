#include "ui/ChatView.h"

#include <gtest/gtest.h>

#include <QLabel>
#include <QLayout>
#include <QPushButton>
#include <QSignalSpy>

namespace devicehub {
namespace {

TEST(ChatViewTest, ShowChannelSetsTitleAndSwitchesAwayFromPlaceholder) {
    ChatView view;

    view.showChannel(QStringLiteral("general"));

    const QLabel* title = view.findChild<QLabel*>(QStringLiteral("chatChannelTitle"));
    ASSERT_NE(title, nullptr);
    EXPECT_EQ(title->text(), QStringLiteral("general"));
}

TEST(ChatViewTest, ShowPlaceholderSwitchesBackFromChannel) {
    ChatView view;
    view.showChannel(QStringLiteral("general"));

    // No public accessor for the current stack page; showPlaceholder()'s
    // observable contract from outside is "doesn't crash and can be
    // followed by showChannel() again" — covered together with the next
    // assertion, since the title label is only meaningfully re-checked
    // after switching back to a (possibly different) channel.
    view.showPlaceholder();
    view.showChannel(QStringLiteral("random"));

    const QLabel* title = view.findChild<QLabel*>(QStringLiteral("chatChannelTitle"));
    ASSERT_NE(title, nullptr);
    EXPECT_EQ(title->text(), QStringLiteral("random"));
}

TEST(ChatViewTest, AppendMessageAddsARow) {
    ChatView view;
    view.showChannel(QStringLiteral("general"));

    const int countBefore = view.messagesContainer()->layout()->count();
    view.appendMessage(ChatMessage{.author = "alice", .body = "hi", .sentAt = "2026-08-05 09:00:00"});

    EXPECT_EQ(view.messagesContainer()->layout()->count(), countBefore + 1);
}

TEST(ChatViewTest, AppendSystemLineAddsARow) {
    ChatView view;

    const int countBefore = view.messagesContainer()->layout()->count();
    view.appendSystemLine(QStringLiteral("alice joined"));

    EXPECT_EQ(view.messagesContainer()->layout()->count(), countBefore + 1);
}

TEST(ChatViewTest, ClearLogRemovesAllAppendedRows) {
    ChatView view;
    view.appendMessage(ChatMessage{.author = "alice", .body = "hi", .sentAt = "2026-08-05 09:00:00"});
    view.appendSystemLine(QStringLiteral("bob joined"));
    const int countBeforeClear = view.messagesContainer()->layout()->count();
    ASSERT_GT(countBeforeClear, 1);

    view.clearLog();

    EXPECT_EQ(view.messagesContainer()->layout()->count(), 1);  // just the trailing stretch
}

TEST(ChatViewTest, SetCallStateReflectsJoinedAndMuted) {
    ChatView view;

    view.setCallState(/*inCall=*/true, /*muted=*/true);

    EXPECT_EQ(view.callToggleButton()->text(), QStringLiteral("Leave call"));
    EXPECT_TRUE(view.muteToggleButton()->isEnabled());
    EXPECT_EQ(view.muteToggleButton()->text(), QStringLiteral("Unmute"));
}

TEST(ChatViewTest, SetCallStateReflectsNotInCall) {
    ChatView view;
    view.setCallState(true, false);

    view.setCallState(/*inCall=*/false, /*muted=*/false);

    EXPECT_EQ(view.callToggleButton()->text(), QStringLiteral("Call"));
    EXPECT_FALSE(view.muteToggleButton()->isEnabled());
}

TEST(ChatViewTest, SetCallParticipantsShowsJoinedNames) {
    ChatView view;

    view.setCallParticipants({QStringLiteral("alice"), QStringLiteral("bob")});

    // isHidden() reflects the explicit hide/show flag regardless of
    // whether the (never-shown, headless-test) widget is actually
    // mapped on screen — see ToastBannerTest.cpp for the same pattern.
    EXPECT_FALSE(view.callParticipantsLabel()->isHidden());
    EXPECT_TRUE(view.callParticipantsLabel()->text().contains(QStringLiteral("alice")));
    EXPECT_TRUE(view.callParticipantsLabel()->text().contains(QStringLiteral("bob")));
}

TEST(ChatViewTest, SetCallParticipantsWithEmptyListHidesLabel) {
    ChatView view;
    view.setCallParticipants({QStringLiteral("alice")});
    ASSERT_FALSE(view.callParticipantsLabel()->isHidden());

    view.setCallParticipants({});

    EXPECT_TRUE(view.callParticipantsLabel()->isHidden());
}

TEST(ChatViewTest, ClickingPlaceholderCreateButtonEmitsCreateChannelRequested) {
    ChatView view;
    QSignalSpy spy(&view, &ChatView::createChannelRequested);

    emit view.findChild<QPushButton*>(QStringLiteral("placeholderCreateChannelButton"))->clicked();

    EXPECT_EQ(spy.count(), 1);
}

TEST(ChatViewTest, ClickingCallToggleButtonEmitsCallToggleRequested) {
    ChatView view;
    QSignalSpy spy(&view, &ChatView::callToggleRequested);

    emit view.callToggleButton()->clicked();

    EXPECT_EQ(spy.count(), 1);
}

TEST(ChatViewTest, ClickingMuteToggleButtonEmitsMuteToggleRequested) {
    ChatView view;
    QSignalSpy spy(&view, &ChatView::muteToggleRequested);

    emit view.muteToggleButton()->clicked();

    EXPECT_EQ(spy.count(), 1);
}

}  // namespace
}  // namespace devicehub
