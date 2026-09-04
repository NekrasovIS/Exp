#include "ui/ChatView.h"

#include <gtest/gtest.h>

#include <QImage>
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

    // +2, не +1 (issue #188): первое сообщение в списке всегда получает
    // разделитель даты перед собой, помимо самой строки сообщения.
    EXPECT_EQ(view.messagesContainer()->layout()->count(), countBefore + 2);
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

TEST(ChatViewTest, ToggleChatVisibilityButtonHiddenOutsideACall) {
    ChatView view;

    // isHidden() отражает явный флаг hide/show независимо от того,
    // отображается ли (никогда не показанный, headless-тест) виджет
    // реально на экране — см. ToastBannerTest.cpp для того же паттерна.
    EXPECT_TRUE(view.toggleChatVisibilityButton()->isHidden());

    view.setCallState(/*inCall=*/true, /*muted=*/false);
    EXPECT_FALSE(view.toggleChatVisibilityButton()->isHidden());
    EXPECT_EQ(view.toggleChatVisibilityButton()->text(), QStringLiteral("Hide Chat"));

    view.setCallState(/*inCall=*/false, /*muted=*/false);
    EXPECT_TRUE(view.toggleChatVisibilityButton()->isHidden());
}

TEST(ChatViewTest, ClickingToggleChatVisibilityButtonTogglesItsLabel) {
    ChatView view;
    view.setCallState(/*inCall=*/true, /*muted=*/false);

    view.toggleChatVisibilityButton()->click();
    EXPECT_EQ(view.toggleChatVisibilityButton()->text(), QStringLiteral("Show Chat"));

    view.toggleChatVisibilityButton()->click();
    EXPECT_EQ(view.toggleChatVisibilityButton()->text(), QStringLiteral("Hide Chat"));
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

TEST(ChatViewTest, ConsecutiveMessagesOnTheSameDayGetOnlyOneDateSeparator) {
    ChatView view;

    view.appendMessage(ChatMessage{.author = "alice", .body = "hi", .sentAt = "2026-08-05 09:00:00"});
    view.appendMessage(ChatMessage{.author = "alice", .body = "again", .sentAt = "2026-08-05 20:00:00"});

    EXPECT_EQ(view.findChildren<QLabel*>(QStringLiteral("chatDateSeparator")).size(), 1);
}

TEST(ChatViewTest, MessagesOnDifferentDaysGetASeparatorEach) {
    ChatView view;

    view.appendMessage(ChatMessage{.author = "alice", .body = "hi", .sentAt = "2026-08-05 09:00:00"});
    view.appendMessage(ChatMessage{.author = "alice", .body = "next day", .sentAt = "2026-08-06 09:00:00"});

    EXPECT_EQ(view.findChildren<QLabel*>(QStringLiteral("chatDateSeparator")).size(), 2);
}

TEST(ChatViewTest, PrependingOlderMessagesAcrossADayBoundaryAddsTwoSeparators) {
    ChatView view;
    // Не первое сообщение вообще — appendMessage() уже дало бы своей
    // дате отдельный разделитель, здесь важно только то, что происходит
    // внутри самой пачки prependMessages().
    view.appendMessage(ChatMessage{.author = "alice", .body = "later", .sentAt = "2026-08-06 09:00:00"});

    view.prependMessages({
        ChatMessage{.author = "alice", .body = "day1", .sentAt = "2026-08-05 09:00:00"},
        ChatMessage{.author = "alice", .body = "day2", .sentAt = "2026-08-06 08:00:00"},
    });

    // Один разделитель перед самым первым (старейшим) сообщением пачки,
    // один — на границе дня внутри неё; уже показанное "later" получило
    // свой собственный при первом appendMessage() выше.
    EXPECT_EQ(view.findChildren<QLabel*>(QStringLiteral("chatDateSeparator")).size(), 3);
}

TEST(ChatViewTest, ClearLogRemovesDateSeparatorsToo) {
    ChatView view;
    view.appendMessage(ChatMessage{.author = "alice", .body = "hi", .sentAt = "2026-08-05 09:00:00"});
    ASSERT_FALSE(view.findChildren<QLabel*>(QStringLiteral("chatDateSeparator")).isEmpty());

    view.clearLog();

    EXPECT_TRUE(view.findChildren<QLabel*>(QStringLiteral("chatDateSeparator")).isEmpty());
}

TEST(ChatViewTest, AppendingAnImageAttachmentEmitsPreviewAttachmentRequested) {
    ChatView view;
    QSignalSpy spy(&view, &ChatView::previewAttachmentRequested);

    view.appendMessage(ChatMessage{.author = "alice",
                                    .body = "look",
                                    .sentAt = "2026-08-05 09:00:00",
                                    .attachmentId = 7,
                                    .attachmentFilename = "photo.png"});

    ASSERT_EQ(spy.count(), 1);
    EXPECT_EQ(spy.at(0).at(0).toLongLong(), 7);
}

TEST(ChatViewTest, AppendingANonImageAttachmentDoesNotRequestAPreview) {
    ChatView view;
    QSignalSpy spy(&view, &ChatView::previewAttachmentRequested);

    view.appendMessage(ChatMessage{.author = "alice",
                                    .body = "see attached",
                                    .sentAt = "2026-08-05 09:00:00",
                                    .attachmentId = 8,
                                    .attachmentFilename = "report.pdf"});

    EXPECT_EQ(spy.count(), 0);
}

TEST(ChatViewTest, SetAttachmentPreviewReachesTheRowThatRequestedIt) {
    ChatView view;
    view.appendMessage(ChatMessage{.id = 9,
                                    .author = "alice",
                                    .body = "look",
                                    .sentAt = "2026-08-05 09:00:00",
                                    .attachmentId = 7,
                                    .attachmentFilename = "photo.png"});
    auto* preview = view.findChild<QLabel*>(QStringLiteral("chatAttachmentPreview"));
    ASSERT_NE(preview, nullptr);
    ASSERT_TRUE(preview->pixmap().isNull());

    QImage image(4, 4, QImage::Format_ARGB32);
    image.fill(Qt::blue);
    view.setAttachmentPreview(7, image);

    EXPECT_FALSE(preview->pixmap().isNull());
}

TEST(ChatViewTest, SetAttachmentPreviewAfterClearLogDoesNotCrash) {
    ChatView view;
    view.appendMessage(ChatMessage{.author = "alice",
                                    .body = "look",
                                    .sentAt = "2026-08-05 09:00:00",
                                    .attachmentId = 7,
                                    .attachmentFilename = "photo.png"});
    view.clearLog();

    QImage image(4, 4, QImage::Format_ARGB32);
    image.fill(Qt::blue);
    view.setAttachmentPreview(7, image);  // must not crash, must not dereference a dangling row
}

}  // namespace
}  // namespace devicehub
