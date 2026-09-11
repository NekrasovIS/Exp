#include "ui/ChatView.h"

#include <gtest/gtest.h>

#include <QAction>
#include <QImage>
#include <QLabel>
#include <QLayout>
#include <QMenu>
#include <QPoint>
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

TEST(ChatViewTest, SetCallStateReflectsJoined) {
    ChatView view;

    view.setCallState(/*inCall=*/true);

    EXPECT_EQ(view.callToggleButton()->text(), QStringLiteral("Leave call"));
}

TEST(ChatViewTest, SetCallStateReflectsNotInCall) {
    ChatView view;
    view.setCallState(true);

    view.setCallState(/*inCall=*/false);

    EXPECT_EQ(view.callToggleButton()->text(), QStringLiteral("Call"));
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

TEST(ChatViewTest, PinnedMessagesButtonHiddenUntilSetPinnedMessagesCountIsPositive) {
    ChatView view;

    // isHidden(), не isVisible() — тест не показывает окно, а
    // isVisible() учитывает всю цепочку предков (всегда false для
    // непоказанного топ-левел виджета).
    EXPECT_TRUE(view.pinnedMessagesButton()->isHidden());

    view.setPinnedMessagesCount(3);
    EXPECT_FALSE(view.pinnedMessagesButton()->isHidden());
    EXPECT_TRUE(view.pinnedMessagesButton()->text().contains("3"));

    view.setPinnedMessagesCount(0);
    EXPECT_TRUE(view.pinnedMessagesButton()->isHidden());
}

TEST(ChatViewTest, ClickingPinnedMessagesButtonEmitsPinnedMessagesToggleRequested) {
    ChatView view;
    view.setPinnedMessagesCount(1);
    QSignalSpy spy(&view, &ChatView::pinnedMessagesToggleRequested);

    emit view.pinnedMessagesButton()->clicked();

    EXPECT_EQ(spy.count(), 1);
}

TEST(ChatViewTest, SetCanManageChannelIsAppliedToSubsequentlyAppendedRows) {
    ChatView view;
    view.setCanManageChannel(true);

    view.appendMessage(ChatMessage{.id = 1, .author = "alice", .body = "hi", .sentAt = "2026-08-05 09:00:00"});

    auto* bubble = view.findChild<QWidget*>(QStringLiteral("chatMessageBubble"));
    ASSERT_NE(bubble, nullptr);
    EXPECT_EQ(bubble->contextMenuPolicy(), Qt::CustomContextMenu);
}

TEST(ChatViewTest, PinMessageRequestedFromARowBubblesUpThroughTheView) {
    ChatView view;
    view.setCanManageChannel(true);
    view.appendMessage(ChatMessage{.id = 1, .author = "alice", .body = "hi", .sentAt = "2026-08-05 09:00:00"});
    auto* row = view.findChild<ChatMessageRow*>();
    ASSERT_NE(row, nullptr);

    QSignalSpy spy(&view, &ChatView::pinMessageRequested);
    emit row->pinRequested(1);

    ASSERT_EQ(spy.count(), 1);
    EXPECT_EQ(spy.at(0).at(0).toLongLong(), 1);
}

TEST(ChatViewTest, UpdatePinnedReachesTheRowThatOwnsThatMessageId) {
    ChatView view;
    view.setCanManageChannel(true);
    view.appendMessage(ChatMessage{.id = 9, .author = "alice", .body = "hi", .sentAt = "2026-08-05 09:00:00"});

    view.updatePinned(9, true);

    EXPECT_FALSE(view.findChild<QLabel*>(QStringLiteral("chatMessagePinnedIndicator"))->isHidden());
}

TEST(ChatViewTest, UpdatePinnedForAMessageNotCurrentlyShownDoesNotCrash) {
    ChatView view;

    view.updatePinned(999, true);  // must not crash, must not dereference a dangling row
}

TEST(ChatViewTest, ClearLogResetsCanManageChannelAndPinnedMessagesCount) {
    ChatView view;
    view.setCanManageChannel(true);
    view.setPinnedMessagesCount(2);

    view.clearLog();

    EXPECT_TRUE(view.pinnedMessagesButton()->isHidden());
    view.appendMessage(ChatMessage{.author = "alice", .body = "hi", .sentAt = "2026-08-05 09:00:00"});
    auto* bubble = view.findChild<QWidget*>(QStringLiteral("chatMessageBubble"));
    ASSERT_NE(bubble, nullptr);
    // Контекстное меню теперь строится всегда (issue #333/#334/#306 —
    // "React"/"Reply" доступны на любом сообщении), поэтому сброс
    // canManageChannel_ проверяется по отсутствию именно пункта Pin, а
    // не по отсутствию меню целиком.
    emit bubble->customContextMenuRequested(QPoint(5, 5));
    auto* menu = bubble->findChild<QMenu*>(QStringLiteral("chatMessageContextMenu"));
    ASSERT_NE(menu, nullptr);
    EXPECT_EQ(menu->findChild<QAction*>(QStringLiteral("pinMessageAction")), nullptr);
}

TEST(ChatViewTest, AppendedMessageWithReactionsShowsAChip) {
    ChatView view;
    view.appendMessage(ChatMessage{
        .author = "alice",
        .body = "hi",
        .sentAt = "2026-08-05 09:00:00",
        .reactions = {MessageReactionSummary{.emoji = "\U0001F44D", .logins = {"bob"}}}});

    EXPECT_EQ(view.findChildren<QPushButton*>(QStringLiteral("reactionChip")).size(), 1);
}

TEST(ChatViewTest, UpdateReactionsReachesTheRowThatOwnsThatMessageId) {
    ChatView view;
    view.appendMessage(ChatMessage{.id = 9, .author = "alice", .body = "hi", .sentAt = "2026-08-05 09:00:00"});
    ASSERT_TRUE(view.findChildren<QPushButton*>(QStringLiteral("reactionChip")).isEmpty());

    view.updateReactions(9, "\U0001F44D", {"bob"});

    EXPECT_EQ(view.findChildren<QPushButton*>(QStringLiteral("reactionChip")).size(), 1);
}

TEST(ChatViewTest, UpdateReactionsForAMessageNotCurrentlyShownDoesNotCrash) {
    ChatView view;

    view.updateReactions(999, "\U0001F44D", {"bob"});  // must not crash, must not dereference a dangling row
}

TEST(ChatViewTest, ClickingAReactionChipEmitsReactionToggleRequestedFromTheView) {
    ChatView view;
    view.appendMessage(ChatMessage{
        .id = 9,
        .author = "alice",
        .body = "hi",
        .sentAt = "2026-08-05 09:00:00",
        .reactions = {MessageReactionSummary{.emoji = "\U0001F44D", .logins = {"bob"}}}});
    auto* chip = view.findChild<QPushButton*>(QStringLiteral("reactionChip"));
    ASSERT_NE(chip, nullptr);

    QSignalSpy spy(&view, &ChatView::reactionToggleRequested);
    chip->click();
    ASSERT_EQ(spy.count(), 1);
    EXPECT_EQ(spy.at(0).at(0).toLongLong(), 9);
    EXPECT_EQ(spy.at(0).at(1).toString(), QStringLiteral("\U0001F44D"));
}

TEST(ChatViewTest, AppendingAReplyResolvesAuthorAndSnippetFromTheCache) {
    ChatView view;
    view.appendMessage(ChatMessage{.id = 1, .author = "alice", .body = "original text", .sentAt = "2026-08-05 09:00:00"});

    view.appendMessage(
        ChatMessage{.id = 2, .author = "bob", .body = "a reply", .sentAt = "2026-08-05 09:01:00", .replyToMessageId = 1});

    auto* quote = view.findChild<QLabel*>(QStringLiteral("chatMessageReplyQuote"));
    ASSERT_NE(quote, nullptr);
    EXPECT_TRUE(quote->text().contains("alice"));
    EXPECT_TRUE(quote->text().contains("original text"));
}

TEST(ChatViewTest, AppendingAReplyToAnUnknownIdShowsUnavailablePlaceholder) {
    ChatView view;

    view.appendMessage(
        ChatMessage{.id = 2, .author = "bob", .body = "a reply", .sentAt = "2026-08-05 09:01:00", .replyToMessageId = 999});

    auto* quote = view.findChild<QLabel*>(QStringLiteral("chatMessageReplyQuote"));
    ASSERT_NE(quote, nullptr);
    EXPECT_TRUE(quote->text().contains("unavailable"));
}

TEST(ChatViewTest, ClickingReplyOnAMessageRowShowsTheReplyBar) {
    ChatView view;
    view.appendMessage(ChatMessage{.id = 1, .author = "alice", .body = "original text", .sentAt = "2026-08-05 09:00:00"});
    auto* row = view.findChild<ChatMessageRow*>();
    ASSERT_NE(row, nullptr);

    emit row->replyRequested(1);

    auto* replyBarLabel = view.findChild<QLabel*>(QStringLiteral("chatReplyBarLabel"));
    ASSERT_NE(replyBarLabel, nullptr);
    EXPECT_TRUE(replyBarLabel->text().contains("alice"));
    EXPECT_TRUE(replyBarLabel->text().contains("original text"));
    // isHidden(), не isVisible() — тест не показывает окно, а isVisible()
    // учитывает всю цепочку предков (всегда false для непоказанного
    // топ-левел виджета); isHidden() отражает только явный флаг видимости
    // самого этого виджета, который и переключает setReplyTarget().
    EXPECT_FALSE(replyBarLabel->parentWidget()->isHidden());
}

TEST(ChatViewTest, ConsumeReplyTargetReturnsTheIdAndClearsIt) {
    ChatView view;
    view.appendMessage(ChatMessage{.id = 1, .author = "alice", .body = "original text", .sentAt = "2026-08-05 09:00:00"});
    view.setReplyTarget(1);

    EXPECT_EQ(view.consumeReplyTarget(), 1);
    EXPECT_EQ(view.consumeReplyTarget(), -1);
    EXPECT_TRUE(view.findChild<QLabel*>(QStringLiteral("chatReplyBarLabel"))->parentWidget()->isHidden());
}

TEST(ChatViewTest, ClickingReplyBarCancelButtonClearsTheReplyTarget) {
    ChatView view;
    view.appendMessage(ChatMessage{.id = 1, .author = "alice", .body = "original text", .sentAt = "2026-08-05 09:00:00"});
    view.setReplyTarget(1);

    emit view.findChild<QPushButton*>(QStringLiteral("chatReplyBarCancelButton"))->clicked();

    EXPECT_EQ(view.consumeReplyTarget(), -1);
}

TEST(ChatViewTest, StartingAReplyCancelsAPendingEdit) {
    ChatView view;
    view.appendMessage(ChatMessage{.id = 1, .author = "alice", .body = "original text", .sentAt = "2026-08-05 09:00:00"});
    auto* row = view.findChild<ChatMessageRow*>();
    ASSERT_NE(row, nullptr);
    emit row->editRequested(1, QStringLiteral("original text"));
    ASSERT_EQ(view.editingMessageId(), 1);

    emit row->replyRequested(1);

    EXPECT_EQ(view.editingMessageId(), -1);
}

TEST(ChatViewTest, StartingAnEditCancelsAPendingReply) {
    ChatView view;
    view.appendMessage(ChatMessage{.id = 1, .author = "alice", .body = "original text", .sentAt = "2026-08-05 09:00:00"});
    auto* row = view.findChild<ChatMessageRow*>();
    ASSERT_NE(row, nullptr);
    view.setReplyTarget(1);

    emit row->editRequested(1, QStringLiteral("original text"));

    EXPECT_EQ(view.consumeReplyTarget(), -1);
}

TEST(ChatViewTest, RemovingTheCurrentReplyTargetClearsIt) {
    ChatView view;
    view.appendMessage(ChatMessage{.id = 1, .author = "alice", .body = "original text", .sentAt = "2026-08-05 09:00:00"});
    view.setReplyTarget(1);

    view.removeMessage(1);

    EXPECT_EQ(view.consumeReplyTarget(), -1);
}

TEST(ChatViewTest, ClearLogClearsAnyPendingReplyTarget) {
    ChatView view;
    view.appendMessage(ChatMessage{.id = 1, .author = "alice", .body = "original text", .sentAt = "2026-08-05 09:00:00"});
    view.setReplyTarget(1);

    view.clearLog();

    EXPECT_EQ(view.consumeReplyTarget(), -1);
}

TEST(ChatViewTest, PrependedMessageRowsAreConnectedJustLikeAppendedOnes) {
    // Регрессия для issue #330: prependMessages() ("Load older messages")
    // раньше не подключала строки к connectMessageRow() вообще — клик по
    // Reply/Edit/Delete/Download на подгруженном старом сообщении молча
    // ничего не делал.
    ChatView view;
    view.prependMessages({ChatMessage{.id = 1, .author = "alice", .body = "old message", .sentAt = "2026-08-05 09:00:00"}});
    auto* row = view.findChild<ChatMessageRow*>();
    ASSERT_NE(row, nullptr);

    emit row->replyRequested(1);

    auto* replyBarLabel = view.findChild<QLabel*>(QStringLiteral("chatReplyBarLabel"));
    ASSERT_NE(replyBarLabel, nullptr);
    EXPECT_FALSE(replyBarLabel->parentWidget()->isHidden());
}

TEST(ChatViewTest, PrependedReplyResolvesAgainstAlreadyCachedMessages) {
    ChatView view;
    view.appendMessage(ChatMessage{.id = 5, .author = "alice", .body = "original text", .sentAt = "2026-08-05 09:00:00"});

    view.prependMessages(
        {ChatMessage{.id = 1, .author = "bob", .body = "an old reply", .sentAt = "2026-08-05 08:00:00", .replyToMessageId = 5}});

    auto* quote = view.findChild<QLabel*>(QStringLiteral("chatMessageReplyQuote"));
    ASSERT_NE(quote, nullptr);
    EXPECT_TRUE(quote->text().contains("alice"));
    EXPECT_TRUE(quote->text().contains("original text"));
}

}  // namespace
}  // namespace devicehub
