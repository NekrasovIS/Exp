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

TEST(ChatViewTest, SetEncryptedAddsLockPrefixAndDisablesAttachAndSearch) {
    ChatView view;
    view.showChannel(QStringLiteral("secret"));

    view.setEncrypted(true);

    const QLabel* title = view.findChild<QLabel*>(QStringLiteral("chatChannelTitle"));
    ASSERT_NE(title, nullptr);
    EXPECT_TRUE(title->text().endsWith(QStringLiteral("secret")));
    EXPECT_NE(title->text(), QStringLiteral("secret"));  // присутствует префикс с замком
    EXPECT_FALSE(view.attachButton()->isEnabled());
    EXPECT_FALSE(view.searchButton()->isEnabled());
}

TEST(ChatViewTest, SetEncryptedFalseRestoresPlainTitleAndReenablesButtons) {
    ChatView view;
    view.showChannel(QStringLiteral("general"));
    view.setEncrypted(true);

    view.setEncrypted(false);

    const QLabel* title = view.findChild<QLabel*>(QStringLiteral("chatChannelTitle"));
    ASSERT_NE(title, nullptr);
    EXPECT_EQ(title->text(), QStringLiteral("general"));
    EXPECT_TRUE(view.attachButton()->isEnabled());
    EXPECT_TRUE(view.searchButton()->isEnabled());
}

TEST(ChatViewTest, ShowPlaceholderSwitchesBackFromChannel) {
    ChatView view;
    view.showChannel(QStringLiteral("general"));

    // Публичного метода доступа к текущей странице стека нет; наблюдаемый
    // извне контракт showPlaceholder() — «не падает и после него можно снова
    // вызвать showChannel()» — проверяется вместе со следующим assert'ом,
    // поскольку метку заголовка осмысленно перепроверять только после
    // переключения обратно на (возможно, другой) канал.
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

    EXPECT_EQ(view.messagesContainer()->layout()->count(), 1);  // остался только завершающий stretch
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

    // isHidden() отражает явный флаг hide/show независимо от того,
    // отображается ли (никогда не показываемый, в headless-тесте) виджет
    // реально на экране — тот же паттерн см. в ToastBannerTest.cpp.
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

TEST(ChatViewTest, ClickingSearchButtonEmitsOpenSearchRequested) {
    ChatView view;
    QSignalSpy spy(&view, &ChatView::openSearchRequested);

    emit view.searchButton()->clicked();

    EXPECT_EQ(spy.count(), 1);
}

TEST(ChatViewTest, ScrollToMessageReturnsTrueForALoadedMessageAndFalseOtherwise) {
    ChatView view;
    view.appendMessage(ChatMessage{.id = 5, .author = "alice", .body = "hi", .sentAt = "2026-08-05 09:00:00"});

    EXPECT_TRUE(view.scrollToMessage(5));
    EXPECT_FALSE(view.scrollToMessage(999));
}

}  // namespace
}  // namespace devicehub
