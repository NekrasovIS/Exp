#include "ui/ChatMessageRow.h"

#include <gtest/gtest.h>

#include <QAction>
#include <QBuffer>
#include <QByteArray>
#include <QImage>
#include <QLabel>
#include <QMenu>
#include <QPoint>
#include <QPushButton>
#include <QSignalSpy>
#include <QUrl>
#include <QWidget>

#include "user/AvatarCache.h"
#include "user/UserProfileClient.h"

namespace devicehub {
namespace {

QByteArray encodeTinyRedPng() {
    QImage image(64, 64, QImage::Format_ARGB32);
    image.fill(Qt::red);
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return bytes;
}

/// Единственный QLabel без имени/свойства "chatAuthor" среди строки с
/// showHeader true — тот же приём различения, что и в
/// NonOwnMessageWithHeaderHasAvatarAuthorAndTimeLabels ниже (аватар
/// умышленно безымянный).
QLabel* findAvatarLabel(const ChatMessageRow& row) {
    for (QLabel* label : row.findChildren<QLabel*>()) {
        if (label->objectName().isEmpty() && !label->property("chatAuthor").toBool()) {
            return label;
        }
    }
    return nullptr;
}

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

ChatMessage sampleMessageWithImageAttachment() {
    return ChatMessage{.author = "alice",
                        .body = "look at this",
                        .sentAt = "2026-08-05 09:00:00",
                        .attachmentId = 43,
                        .attachmentFilename = "photo.PNG"};
}

ChatMessage sampleMessageWithVideoAttachment() {
    return ChatMessage{.author = "alice",
                        .body = "watch this",
                        .sentAt = "2026-08-05 09:00:00",
                        .attachmentId = 44,
                        .attachmentFilename = "clip.mp4"};
}

ChatMessage sampleMessageWithAudioAttachment() {
    return ChatMessage{.author = "alice",
                        .body = "",
                        .sentAt = "2026-08-05 09:00:00",
                        .attachmentId = 45,
                        .attachmentFilename = "voice-message-123.wav"};
}

ChatMessage sampleMessageWithUrl() {
    return ChatMessage{
        .author = "alice", .body = "check this out: https://example.test/article", .sentAt = "2026-08-05 09:00:00"};
}

TEST(ChatMessageRowTest, NonOwnMessageWithHeaderHasAvatarAuthorAndTimeLabels) {
    ChatMessageRow row(sampleMessage(), /*showHeader=*/true, /*isOwnMessage=*/false);

    // Аватар (безымянный QLabel) + автор (свойство "chatAuthor") + время
    // ("mutedDescription") + индикатор "Pinned" (issue #338, всегда
    // создаётся, просто скрыт — см. rebuildReactionChips-подобный приём
    // у pinnedIndicatorLabel_) + текст = 5 QLabel; у собственных сообщений
    // никогда нет метки автора (см. NonOwnMessageWithHeader ниже) — именно
    // это отличает данный счётчик от случая собственного сообщения.
    EXPECT_EQ(row.findChildren<QLabel*>().size(), 5);
    int authorLabelCount = 0;
    for (const QLabel* label : row.findChildren<QLabel*>()) {
        if (label->property("chatAuthor").toBool()) {
            ++authorLabelCount;
        }
    }
    EXPECT_EQ(authorLabelCount, 1);
}

// Issue #384/#442 — реальное фото автора вместо буквы-заглушки.

TEST(ChatMessageRowTest, AvatarUsesTheLetterPlaceholderWhenNoAvatarCacheIsGiven) {
    // avatarCache по умолчанию nullptr — прежнее поведение, важное для
    // всех остальных тестов этого файла, которые не передают его вовсе.
    ChatMessageRow row(sampleMessage(), /*showHeader=*/true, /*isOwnMessage=*/false);

    QLabel* avatarLabel = findAvatarLabel(row);
    ASSERT_NE(avatarLabel, nullptr);
    EXPECT_FALSE(avatarLabel->pixmap().isNull());
}

TEST(ChatMessageRowTest, AvatarShowsARealPhotoAlreadyCachedForTheAuthorAtConstruction) {
    UserProfileClient client(QUrl(QStringLiteral("http://127.0.0.1:1")));
    AvatarCache cache(client);
    client.avatarFetched(QStringLiteral("alice"), encodeTinyRedPng(), QStringLiteral("image/png"));

    ChatMessageRow rowWithCache(sampleMessage(), /*showHeader=*/true, /*isOwnMessage=*/false, QString(),
                                 /*canManageChannel=*/false, &cache);
    ChatMessageRow rowWithoutCache(sampleMessage(), /*showHeader=*/true, /*isOwnMessage=*/false);

    QLabel* withCache = findAvatarLabel(rowWithCache);
    QLabel* withoutCache = findAvatarLabel(rowWithoutCache);
    ASSERT_NE(withCache, nullptr);
    ASSERT_NE(withoutCache, nullptr);
    EXPECT_NE(withCache->pixmap().toImage(), withoutCache->pixmap().toImage());
}

TEST(ChatMessageRowTest, AvatarSwapsInTheRealPhotoOnceTheCacheLoadsItAfterConstruction) {
    UserProfileClient client(QUrl(QStringLiteral("http://127.0.0.1:1")));
    AvatarCache cache(client);
    ChatMessageRow row(sampleMessage(), /*showHeader=*/true, /*isOwnMessage=*/false, QString(),
                        /*canManageChannel=*/false, &cache);
    QLabel* avatarLabel = findAvatarLabel(row);
    ASSERT_NE(avatarLabel, nullptr);
    const QImage letterPixmap = avatarLabel->pixmap().toImage();

    client.avatarFetched(QStringLiteral("alice"), encodeTinyRedPng(), QStringLiteral("image/png"));

    EXPECT_NE(avatarLabel->pixmap().toImage(), letterPixmap);
}

TEST(ChatMessageRowTest, OwnMessageWithHeaderHasNoAuthorLabel) {
    ChatMessageRow row(sampleMessage(), /*showHeader=*/true, /*isOwnMessage=*/true);

    // Время + индикатор "Pinned" (всегда создаётся, скрыт — issue #338)
    // + текст + "Seen by" (всегда создаётся для своих сообщений, скрыт
    // — issue #380) — у собственных сообщений нет аватара и метки автора.
    EXPECT_EQ(row.findChildren<QLabel*>().size(), 4);
    for (const QLabel* label : row.findChildren<QLabel*>()) {
        EXPECT_FALSE(label->property("chatAuthor").toBool());
    }
}

TEST(ChatMessageRowTest, GroupedMessageWithoutHeaderHasOnlyBodyAndPinnedIndicatorLabels) {
    ChatMessageRow row(sampleMessage(), /*showHeader=*/false, /*isOwnMessage=*/false);

    // Текст + индикатор "Pinned" (всегда создаётся, скрыт — issue #338).
    EXPECT_EQ(row.findChildren<QLabel*>().size(), 2);
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

TEST(ChatMessageRowTest, NonOwnMessageContextMenuHasReactButNotEditOrDelete) {
    ChatMessageRow row(sampleMessage(), /*showHeader=*/true, /*isOwnMessage=*/false);

    auto* bubble = row.findChild<QWidget*>(QStringLiteral("chatMessageBubble"));
    ASSERT_NE(bubble, nullptr);
    EXPECT_EQ(bubble->contextMenuPolicy(), Qt::CustomContextMenu);

    emit bubble->customContextMenuRequested(QPoint(5, 5));
    auto* menu = bubble->findChild<QMenu*>(QStringLiteral("chatMessageContextMenu"));
    ASSERT_NE(menu, nullptr);
    EXPECT_NE(menu->findChild<QMenu*>(QStringLiteral("reactMessageMenu")), nullptr);
    EXPECT_EQ(menu->findChild<QAction*>(QStringLiteral("editMessageAction")), nullptr);
    EXPECT_EQ(menu->findChild<QAction*>(QStringLiteral("deleteMessageAction")), nullptr);
}

TEST(ChatMessageRowTest, ReactMenuActionEmitsReactionToggleRequestedWithItsEmoji) {
    ChatMessageRow row(sampleMessage(), /*showHeader=*/true, /*isOwnMessage=*/false);

    auto* bubble = row.findChild<QWidget*>(QStringLiteral("chatMessageBubble"));
    ASSERT_NE(bubble, nullptr);
    emit bubble->customContextMenuRequested(QPoint(5, 5));
    auto* menu = bubble->findChild<QMenu*>(QStringLiteral("chatMessageContextMenu"));
    ASSERT_NE(menu, nullptr);
    auto* reactMenu = menu->findChild<QMenu*>(QStringLiteral("reactMessageMenu"));
    ASSERT_NE(reactMenu, nullptr);
    const QList<QAction*> reactionActions = reactMenu->findChildren<QAction*>(QStringLiteral("reactionMenuAction"));
    ASSERT_FALSE(reactionActions.isEmpty());

    QSignalSpy spy(&row, &ChatMessageRow::reactionToggleRequested);
    reactionActions.first()->trigger();
    ASSERT_EQ(spy.count(), 1);
    EXPECT_EQ(spy.at(0).at(0).toLongLong(), row.messageId());
    EXPECT_EQ(spy.at(0).at(1).toString(), reactionActions.first()->text());
}

TEST(ChatMessageRowTest, OwnMessageContextMenuAlsoHasReactSubmenu) {
    ChatMessageRow row(sampleMessage(), /*showHeader=*/true, /*isOwnMessage=*/true);

    auto* bubble = row.findChild<QWidget*>(QStringLiteral("chatMessageBubble"));
    ASSERT_NE(bubble, nullptr);
    emit bubble->customContextMenuRequested(QPoint(5, 5));
    auto* menu = bubble->findChild<QMenu*>(QStringLiteral("chatMessageContextMenu"));
    ASSERT_NE(menu, nullptr);
    EXPECT_NE(menu->findChild<QMenu*>(QStringLiteral("reactMessageMenu")), nullptr);
}

TEST(ChatMessageRowTest, MessageWithNoReactionsHasNoVisibleReactionChips) {
    ChatMessageRow row(sampleMessage(), /*showHeader=*/true, /*isOwnMessage=*/false);

    EXPECT_TRUE(row.findChildren<QPushButton*>(QStringLiteral("reactionChip")).isEmpty());
}

TEST(ChatMessageRowTest, MessageConstructedWithReactionsShowsAChipPerEmojiWithCountAndTooltip) {
    ChatMessage message = sampleMessage();
    message.reactions = {MessageReactionSummary{.emoji = "\U0001F44D", .logins = {"bob", "carol"}}};
    ChatMessageRow row(message, /*showHeader=*/true, /*isOwnMessage=*/false);

    const QList<QPushButton*> chips = row.findChildren<QPushButton*>(QStringLiteral("reactionChip"));
    ASSERT_EQ(chips.size(), 1);
    EXPECT_TRUE(chips.first()->text().contains("2"));
    EXPECT_EQ(chips.first()->toolTip(), QStringLiteral("bob, carol"));
}

TEST(ChatMessageRowTest, ClickingAReactionChipEmitsReactionToggleRequested) {
    ChatMessage message = sampleMessage();
    message.reactions = {MessageReactionSummary{.emoji = "\U0001F44D", .logins = {"bob"}}};
    ChatMessageRow row(message, /*showHeader=*/true, /*isOwnMessage=*/false);
    auto* chip = row.findChild<QPushButton*>(QStringLiteral("reactionChip"));
    ASSERT_NE(chip, nullptr);

    QSignalSpy spy(&row, &ChatMessageRow::reactionToggleRequested);
    chip->click();
    ASSERT_EQ(spy.count(), 1);
    EXPECT_EQ(spy.at(0).at(0).toLongLong(), row.messageId());
    EXPECT_EQ(spy.at(0).at(1).toString(), QStringLiteral("\U0001F44D"));
}

TEST(ChatMessageRowTest, OwnReactionChipHasOwnReactionPropertySetWhenCurrentUserIsInLogins) {
    ChatMessage message = sampleMessage();
    message.reactions = {MessageReactionSummary{.emoji = "\U0001F44D", .logins = {"bob", "carol"}}};
    ChatMessageRow row(message, /*showHeader=*/true, /*isOwnMessage=*/false, /*currentUserLogin=*/"carol");

    auto* chip = row.findChild<QPushButton*>(QStringLiteral("reactionChip"));
    ASSERT_NE(chip, nullptr);
    EXPECT_TRUE(chip->property("ownReaction").toBool());
}

TEST(ChatMessageRowTest, ApplyReactionChangeAddsAndUpdatesAChip) {
    ChatMessageRow row(sampleMessage(), /*showHeader=*/true, /*isOwnMessage=*/false);
    ASSERT_TRUE(row.findChildren<QPushButton*>(QStringLiteral("reactionChip")).isEmpty());

    row.applyReactionChange("\U0001F44D", {"bob"});
    QList<QPushButton*> chips = row.findChildren<QPushButton*>(QStringLiteral("reactionChip"));
    ASSERT_EQ(chips.size(), 1);
    EXPECT_TRUE(chips.first()->text().contains("1"));

    row.applyReactionChange("\U0001F44D", {"bob", "carol"});
    chips = row.findChildren<QPushButton*>(QStringLiteral("reactionChip"));
    ASSERT_EQ(chips.size(), 1);
    EXPECT_TRUE(chips.first()->text().contains("2"));
}

TEST(ChatMessageRowTest, ApplyReactionChangeWithEmptyLoginsRemovesTheChip) {
    ChatMessage message = sampleMessage();
    message.reactions = {MessageReactionSummary{.emoji = "\U0001F44D", .logins = {"bob"}}};
    ChatMessageRow row(message, /*showHeader=*/true, /*isOwnMessage=*/false);
    ASSERT_EQ(row.findChildren<QPushButton*>(QStringLiteral("reactionChip")).size(), 1);

    row.applyReactionChange("\U0001F44D", {});

    EXPECT_TRUE(row.findChildren<QPushButton*>(QStringLiteral("reactionChip")).isEmpty());
}

TEST(ChatMessageRowTest, ApplyReactionChangeDoesNotAffectOtherEmojis) {
    ChatMessage message = sampleMessage();
    message.reactions = {MessageReactionSummary{.emoji = "\U0001F44D", .logins = {"bob"}},
                          MessageReactionSummary{.emoji = "\U0001F389", .logins = {"carol"}}};
    ChatMessageRow row(message, /*showHeader=*/true, /*isOwnMessage=*/false);

    row.applyReactionChange("\U0001F44D", {});

    const QList<QPushButton*> chips = row.findChildren<QPushButton*>(QStringLiteral("reactionChip"));
    ASSERT_EQ(chips.size(), 1);
    EXPECT_TRUE(chips.first()->text().contains("\U0001F389"));
}

TEST(ChatMessageRowTest, NonOwnMessageContextMenuHasOnlyReplyNotEditOrDelete) {
    ChatMessageRow row(sampleMessage(), /*showHeader=*/true, /*isOwnMessage=*/false);

    auto* bubble = row.findChild<QWidget*>(QStringLiteral("chatMessageBubble"));
    ASSERT_NE(bubble, nullptr);
    EXPECT_EQ(bubble->contextMenuPolicy(), Qt::CustomContextMenu);

    emit bubble->customContextMenuRequested(QPoint(5, 5));
    auto* menu = bubble->findChild<QMenu*>(QStringLiteral("chatMessageContextMenu"));
    ASSERT_NE(menu, nullptr);
    EXPECT_NE(menu->findChild<QAction*>(QStringLiteral("replyMessageAction")), nullptr);
    EXPECT_EQ(menu->findChild<QAction*>(QStringLiteral("editMessageAction")), nullptr);
    EXPECT_EQ(menu->findChild<QAction*>(QStringLiteral("deleteMessageAction")), nullptr);
}

TEST(ChatMessageRowTest, NonOwnMessageContextMenuReplyActionEmitsReplyRequested) {
    ChatMessageRow row(sampleMessage(), /*showHeader=*/true, /*isOwnMessage=*/false);

    auto* bubble = row.findChild<QWidget*>(QStringLiteral("chatMessageBubble"));
    ASSERT_NE(bubble, nullptr);
    emit bubble->customContextMenuRequested(QPoint(5, 5));
    auto* menu = bubble->findChild<QMenu*>(QStringLiteral("chatMessageContextMenu"));
    ASSERT_NE(menu, nullptr);
    auto* replyAction = menu->findChild<QAction*>(QStringLiteral("replyMessageAction"));
    ASSERT_NE(replyAction, nullptr);

    QSignalSpy spy(&row, &ChatMessageRow::replyRequested);
    replyAction->trigger();
    ASSERT_EQ(spy.count(), 1);
    EXPECT_EQ(spy.at(0).at(0).toLongLong(), row.messageId());
}

TEST(ChatMessageRowTest, OwnMessageContextMenuAlsoHasReplyAction) {
    ChatMessageRow row(sampleMessage(), /*showHeader=*/true, /*isOwnMessage=*/true);

    auto* bubble = row.findChild<QWidget*>(QStringLiteral("chatMessageBubble"));
    ASSERT_NE(bubble, nullptr);
    emit bubble->customContextMenuRequested(QPoint(5, 5));
    auto* menu = bubble->findChild<QMenu*>(QStringLiteral("chatMessageContextMenu"));
    ASSERT_NE(menu, nullptr);
    EXPECT_NE(menu->findChild<QAction*>(QStringLiteral("replyMessageAction")), nullptr);
}

TEST(ChatMessageRowTest, MessageWithoutReplyHasNoQuoteLabel) {
    ChatMessageRow row(sampleMessage(), /*showHeader=*/true, /*isOwnMessage=*/false);

    EXPECT_EQ(row.findChild<QLabel*>("chatMessageReplyQuote"), nullptr);
}

TEST(ChatMessageRowTest, MessageWithResolvedReplyShowsAuthorAndSnippetInQuote) {
    ChatMessage message = sampleMessage();
    message.replyToMessageId = 7;
    message.replyToAuthor = "bob";
    message.replyToBodySnippet = "original text";
    ChatMessageRow row(message, /*showHeader=*/true, /*isOwnMessage=*/false);

    auto* quote = row.findChild<QLabel*>("chatMessageReplyQuote");
    ASSERT_NE(quote, nullptr);
    EXPECT_TRUE(quote->text().contains("bob"));
    EXPECT_TRUE(quote->text().contains("original text"));
}

TEST(ChatMessageRowTest, MessageWithUnresolvedReplyShowsUnavailablePlaceholder) {
    ChatMessage message = sampleMessage();
    message.replyToMessageId = 7;
    ChatMessageRow row(message, /*showHeader=*/true, /*isOwnMessage=*/false);

    auto* quote = row.findChild<QLabel*>("chatMessageReplyQuote");
    ASSERT_NE(quote, nullptr);
    EXPECT_TRUE(quote->text().contains("unavailable"));
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

TEST(ChatMessageRowTest, WithoutCanManageChannelNonOwnMessageHasNoPinActionAndNoPinnedIndicator) {
    // Контекстное меню теперь строится всегда (issue #333/#334/#306 —
    // "React"/"Reply" доступны на любом сообщении), поэтому
    // canManageChannel=false больше не означает "меню вообще нет", как
    // раньше в #339 в одиночку — только "нет пункта Pin в этом меню".
    ChatMessageRow row(sampleMessage(), /*showHeader=*/true, /*isOwnMessage=*/false, /*currentUserLogin=*/QString(),
                        /*canManageChannel=*/false);

    auto* bubble = row.findChild<QWidget*>(QStringLiteral("chatMessageBubble"));
    ASSERT_NE(bubble, nullptr);
    EXPECT_EQ(bubble->contextMenuPolicy(), Qt::CustomContextMenu);

    emit bubble->customContextMenuRequested(QPoint(5, 5));
    auto* menu = bubble->findChild<QMenu*>(QStringLiteral("chatMessageContextMenu"));
    ASSERT_NE(menu, nullptr);
    EXPECT_EQ(menu->findChild<QAction*>(QStringLiteral("pinMessageAction")), nullptr);
    // isHidden(), не isVisible() — тест не показывает окно, а
    // isVisible() учитывает всю цепочку предков (всегда false для
    // непоказанного топ-левел виджета); isHidden() отражает только
    // явный флаг видимости самого этого виджета.
    EXPECT_TRUE(row.findChild<QLabel*>(QStringLiteral("chatMessagePinnedIndicator"))->isHidden());
}

TEST(ChatMessageRowTest, CanManageChannelShowsPinActionOnNonOwnMessageButNotEditOrDelete) {
    ChatMessageRow row(sampleMessage(), /*showHeader=*/true, /*isOwnMessage=*/false, /*currentUserLogin=*/QString(), /*canManageChannel=*/true);

    auto* bubble = row.findChild<QWidget*>(QStringLiteral("chatMessageBubble"));
    ASSERT_NE(bubble, nullptr);
    EXPECT_EQ(bubble->contextMenuPolicy(), Qt::CustomContextMenu);

    emit bubble->customContextMenuRequested(QPoint(5, 5));
    auto* menu = bubble->findChild<QMenu*>(QStringLiteral("chatMessageContextMenu"));
    ASSERT_NE(menu, nullptr);
    auto* pinAction = menu->findChild<QAction*>(QStringLiteral("pinMessageAction"));
    ASSERT_NE(pinAction, nullptr);
    EXPECT_EQ(pinAction->text(), QStringLiteral("Pin"));
    EXPECT_EQ(menu->findChild<QAction*>(QStringLiteral("editMessageAction")), nullptr);
    EXPECT_EQ(menu->findChild<QAction*>(QStringLiteral("deleteMessageAction")), nullptr);
}

TEST(ChatMessageRowTest, PinActionOnAnUnpinnedMessageEmitsPinRequestedAndSetPinnedShowsTheIndicator) {
    ChatMessageRow row(sampleMessage(), /*showHeader=*/true, /*isOwnMessage=*/false, /*currentUserLogin=*/QString(), /*canManageChannel=*/true);
    auto* bubble = row.findChild<QWidget*>(QStringLiteral("chatMessageBubble"));
    ASSERT_NE(bubble, nullptr);

    emit bubble->customContextMenuRequested(QPoint(5, 5));
    auto* menu = bubble->findChild<QMenu*>(QStringLiteral("chatMessageContextMenu"));
    ASSERT_NE(menu, nullptr);
    auto* pinAction = menu->findChild<QAction*>(QStringLiteral("pinMessageAction"));
    ASSERT_NE(pinAction, nullptr);
    EXPECT_EQ(pinAction->text(), QStringLiteral("Pin"));

    QSignalSpy pinSpy(&row, &ChatMessageRow::pinRequested);
    pinAction->trigger();
    ASSERT_EQ(pinSpy.count(), 1);
    EXPECT_EQ(pinSpy.at(0).at(0).toLongLong(), row.messageId());

    // setPinned() пришло бы от ChatView не раньше, чем сервер подтвердит
    // это через ChatClient::messagePinned() — а не оптимистично сразу
    // после клика (тот же принцип "без локального оптимистичного
    // обновления", что и у edit/delete).
    row.setPinned(true);
    EXPECT_FALSE(row.findChild<QLabel*>(QStringLiteral("chatMessagePinnedIndicator"))->isHidden());
}

TEST(ChatMessageRowTest, PinActionOnAnAlreadyPinnedMessageShowsUnpinAndEmitsUnpinRequested) {
    // Отдельная строка, а не повторный правый клик на той же (issue
    // #150 уже установил стиль "новый QMenu на каждый клик, через
    // popup(), не exec()") — в headless-тесте старое QMenu никогда
    // реально не закрывается (popup() не блокирует, событие закрытия не
    // приходит), поэтому второй правый клик на той же строке нашёл бы
    // ещё живой ПЕРВЫЙ QMenu через findChild(), а не только что
    // созданный.
    ChatMessage message = sampleMessage();
    message.isPinned = true;
    ChatMessageRow row(message, /*showHeader=*/true, /*isOwnMessage=*/false, /*currentUserLogin=*/QString(), /*canManageChannel=*/true);
    auto* bubble = row.findChild<QWidget*>(QStringLiteral("chatMessageBubble"));
    ASSERT_NE(bubble, nullptr);

    emit bubble->customContextMenuRequested(QPoint(5, 5));
    auto* menu = bubble->findChild<QMenu*>(QStringLiteral("chatMessageContextMenu"));
    ASSERT_NE(menu, nullptr);
    auto* unpinAction = menu->findChild<QAction*>(QStringLiteral("pinMessageAction"));
    ASSERT_NE(unpinAction, nullptr);
    EXPECT_EQ(unpinAction->text(), QStringLiteral("Unpin"));

    QSignalSpy unpinSpy(&row, &ChatMessageRow::unpinRequested);
    unpinAction->trigger();
    ASSERT_EQ(unpinSpy.count(), 1);
    EXPECT_EQ(unpinSpy.at(0).at(0).toLongLong(), row.messageId());
}

TEST(ChatMessageRowTest, MessageConstructedAlreadyPinnedShowsIndicatorImmediately) {
    ChatMessage message = sampleMessage();
    message.isPinned = true;
    ChatMessageRow row(message, /*showHeader=*/true, /*isOwnMessage=*/false, /*currentUserLogin=*/QString(), /*canManageChannel=*/true);

    EXPECT_FALSE(row.findChild<QLabel*>(QStringLiteral("chatMessagePinnedIndicator"))->isHidden());
}

TEST(ChatMessageRowTest, OwnMessageWithCanManageChannelHasEditPinAndDeleteAllTogether) {
    ChatMessageRow row(sampleMessage(), /*showHeader=*/true, /*isOwnMessage=*/true, /*currentUserLogin=*/QString(), /*canManageChannel=*/true);
    auto* bubble = row.findChild<QWidget*>(QStringLiteral("chatMessageBubble"));
    ASSERT_NE(bubble, nullptr);

    emit bubble->customContextMenuRequested(QPoint(5, 5));
    auto* menu = bubble->findChild<QMenu*>(QStringLiteral("chatMessageContextMenu"));
    ASSERT_NE(menu, nullptr);
    EXPECT_NE(menu->findChild<QAction*>(QStringLiteral("editMessageAction")), nullptr);
    EXPECT_NE(menu->findChild<QAction*>(QStringLiteral("pinMessageAction")), nullptr);
    EXPECT_NE(menu->findChild<QAction*>(QStringLiteral("deleteMessageAction")), nullptr);
}

TEST(ChatMessageRowTest, IsImageAttachmentRecognizesKnownImageExtensionsCaseInsensitively) {
    EXPECT_TRUE(isImageAttachment(QStringLiteral("photo.png")));
    EXPECT_TRUE(isImageAttachment(QStringLiteral("photo.PNG")));
    EXPECT_TRUE(isImageAttachment(QStringLiteral("photo.jpeg")));
    EXPECT_TRUE(isImageAttachment(QStringLiteral("animated.gif")));
    EXPECT_FALSE(isImageAttachment(QStringLiteral("report.pdf")));
    EXPECT_FALSE(isImageAttachment(QStringLiteral("clip.mp4")));
}

TEST(ChatMessageRowTest, IsVideoAttachmentRecognizesKnownVideoExtensionsCaseInsensitively) {
    EXPECT_TRUE(isVideoAttachment(QStringLiteral("clip.mp4")));
    EXPECT_TRUE(isVideoAttachment(QStringLiteral("clip.MOV")));
    EXPECT_FALSE(isVideoAttachment(QStringLiteral("photo.png")));
    EXPECT_FALSE(isVideoAttachment(QStringLiteral("report.pdf")));
}

TEST(ChatMessageRowTest, NonImageNonVideoAttachmentHasNoPreviewOrVideoPlaceholder) {
    ChatMessageRow row(sampleMessageWithAttachment(), /*showHeader=*/true, /*isOwnMessage=*/false);

    EXPECT_EQ(row.findChild<QLabel*>("chatAttachmentPreview"), nullptr);
    EXPECT_EQ(row.findChild<QLabel*>("chatAttachmentVideoPlaceholder"), nullptr);
}

TEST(ChatMessageRowTest, ImageAttachmentShowsPlaceholderThenRealPreviewOnceLoaded) {
    ChatMessageRow row(sampleMessageWithImageAttachment(), /*showHeader=*/true, /*isOwnMessage=*/false);

    auto* preview = row.findChild<QLabel*>("chatAttachmentPreview");
    ASSERT_NE(preview, nullptr);
    EXPECT_FALSE(preview->text().isEmpty());
    EXPECT_TRUE(preview->pixmap().isNull());

    QImage image(8, 4, QImage::Format_ARGB32);
    image.fill(Qt::red);
    row.setAttachmentPreview(image);

    EXPECT_TRUE(preview->text().isEmpty());
    EXPECT_FALSE(preview->pixmap().isNull());
}

TEST(ChatMessageRowTest, FailedImagePreviewShowsAFallbackMessage) {
    ChatMessageRow row(sampleMessageWithImageAttachment(), /*showHeader=*/true, /*isOwnMessage=*/false);

    row.setAttachmentPreview(QImage());

    auto* preview = row.findChild<QLabel*>("chatAttachmentPreview");
    ASSERT_NE(preview, nullptr);
    EXPECT_FALSE(preview->text().isEmpty());
    EXPECT_TRUE(preview->pixmap().isNull());
}

TEST(ChatMessageRowTest, SetAttachmentPreviewOnARowWithoutAnImageAttachmentIsANoop) {
    ChatMessageRow row(sampleMessageWithAttachment(), /*showHeader=*/true, /*isOwnMessage=*/false);

    QImage image(8, 4, QImage::Format_ARGB32);
    image.fill(Qt::red);
    row.setAttachmentPreview(image);  // must not crash

    EXPECT_EQ(row.findChild<QLabel*>("chatAttachmentPreview"), nullptr);
}

TEST(ChatMessageRowTest, VideoAttachmentShowsAPlaceholderWithTheFilename) {
    ChatMessageRow row(sampleMessageWithVideoAttachment(), /*showHeader=*/true, /*isOwnMessage=*/false);

    auto* placeholder = row.findChild<QLabel*>("chatAttachmentVideoPlaceholder");
    ASSERT_NE(placeholder, nullptr);
    EXPECT_TRUE(placeholder->text().contains(QStringLiteral("clip.mp4")));
    EXPECT_EQ(row.findChild<QLabel*>("chatAttachmentPreview"), nullptr);
}

TEST(ChatMessageRowTest, IsAudioAttachmentRecognizesWavExtensionOnly) {
    EXPECT_TRUE(isAudioAttachment(QStringLiteral("voice-message-123.wav")));
    EXPECT_TRUE(isAudioAttachment(QStringLiteral("clip.WAV")));
    EXPECT_FALSE(isAudioAttachment(QStringLiteral("clip.mp4")));
    EXPECT_FALSE(isAudioAttachment(QStringLiteral("photo.png")));
}

// Issue #359 — голосовое сообщение показывает Play вместо ссылки
// "Download", в отличие от любого другого вложения: сохранять WAV на
// диск незачем, вложение существует только чтобы быть прослушанным.
TEST(ChatMessageRowTest, AudioAttachmentShowsPlayButtonInsteadOfDownload) {
    ChatMessageRow row(sampleMessageWithAudioAttachment(), /*showHeader=*/true, /*isOwnMessage=*/false);

    auto* playButton = row.findChild<QPushButton*>("playVoiceMessageButton");
    ASSERT_NE(playButton, nullptr);
    EXPECT_TRUE(playButton->text().contains(QStringLiteral("Play")));
    EXPECT_EQ(row.findChild<QPushButton*>("downloadAttachmentButton"), nullptr);
    EXPECT_EQ(row.findChild<QLabel*>("chatAttachmentPreview"), nullptr);
    EXPECT_EQ(row.findChild<QLabel*>("chatAttachmentVideoPlaceholder"), nullptr);
}

TEST(ChatMessageRowTest, ClickingPlayBeforeAudioIsLoadedEmitsPlaybackRequestedAndDisablesTheButton) {
    ChatMessageRow row(sampleMessageWithAudioAttachment(), /*showHeader=*/true, /*isOwnMessage=*/false);
    QSignalSpy spy(&row, &ChatMessageRow::playbackRequested);

    auto* playButton = row.findChild<QPushButton*>("playVoiceMessageButton");
    ASSERT_NE(playButton, nullptr);
    emit playButton->clicked();

    ASSERT_EQ(spy.count(), 1);
    EXPECT_EQ(spy.at(0).at(0).toLongLong(), 45);
    EXPECT_FALSE(playButton->isEnabled());
    EXPECT_TRUE(playButton->text().contains(QStringLiteral("Loading")));
}

TEST(ChatMessageRowTest, SetAudioDataWithEmptyBytesShowsAFallbackMessage) {
    ChatMessageRow row(sampleMessageWithAudioAttachment(), /*showHeader=*/true, /*isOwnMessage=*/false);

    row.setAudioData(QByteArray());

    auto* playButton = row.findChild<QPushButton*>("playVoiceMessageButton");
    ASSERT_NE(playButton, nullptr);
    EXPECT_TRUE(playButton->text().contains(QStringLiteral("unavailable")));
}

TEST(ChatMessageRowTest, SetAudioDataOnARowWithoutAnAudioAttachmentIsANoop) {
    ChatMessageRow row(sampleMessageWithAttachment(), /*showHeader=*/true, /*isOwnMessage=*/false);

    row.setAudioData(QByteArray("not really audio"));  // must not crash

    EXPECT_EQ(row.findChild<QPushButton*>("playVoiceMessageButton"), nullptr);
}

TEST(ChatMessageRowTest, OwnMessageHasAHiddenSeenByLabel) {
    ChatMessageRow row(sampleMessage(), /*showHeader=*/true, /*isOwnMessage=*/true);

    // isHidden(), не isVisible() — тест не показывает окно (см.
    // пояснение выше в файле про PinActionOnAnUnpinnedMessage...).
    EXPECT_TRUE(row.findChild<QLabel*>(QStringLiteral("chatMessageSeenBy"))->isHidden());
}

TEST(ChatMessageRowTest, NonOwnMessageHasNoSeenByLabel) {
    ChatMessageRow row(sampleMessage(), /*showHeader=*/true, /*isOwnMessage=*/false);

    EXPECT_EQ(row.findChild<QLabel*>(QStringLiteral("chatMessageSeenBy")), nullptr);
}

TEST(ChatMessageRowTest, SetSeenByWithNonEmptyListShowsTheLabelWithJoinedNames) {
    ChatMessageRow row(sampleMessage(), /*showHeader=*/true, /*isOwnMessage=*/true);

    row.setSeenBy({"bob", "carol"});

    auto* label = row.findChild<QLabel*>(QStringLiteral("chatMessageSeenBy"));
    ASSERT_NE(label, nullptr);
    EXPECT_FALSE(label->isHidden());
    EXPECT_EQ(label->text(), QStringLiteral("Seen by: bob, carol"));
}

TEST(ChatMessageRowTest, SetSeenByWithEmptyListHidesTheLabelAgain) {
    ChatMessageRow row(sampleMessage(), /*showHeader=*/true, /*isOwnMessage=*/true);
    row.setSeenBy({"bob"});

    row.setSeenBy({});

    EXPECT_TRUE(row.findChild<QLabel*>(QStringLiteral("chatMessageSeenBy"))->isHidden());
}

TEST(ChatMessageRowTest, SetSeenByOnANonOwnMessageIsANoop) {
    ChatMessageRow row(sampleMessage(), /*showHeader=*/true, /*isOwnMessage=*/false);

    row.setSeenBy({"bob"});  // must not crash

    EXPECT_EQ(row.findChild<QLabel*>(QStringLiteral("chatMessageSeenBy")), nullptr);
}

// Issue #396/#398 — link previews.

TEST(ChatMessageRowTest, MessageWithoutAUrlHasNoLinkPreviewFrame) {
    ChatMessageRow row(sampleMessage(), /*showHeader=*/true, /*isOwnMessage=*/false);

    EXPECT_EQ(row.findChild<QWidget*>("chatLinkPreview"), nullptr);
}

TEST(ChatMessageRowTest, MessageWithAUrlHasAHiddenLinkPreviewFrameShowingTheDomain) {
    ChatMessageRow row(sampleMessageWithUrl(), /*showHeader=*/true, /*isOwnMessage=*/false);

    auto* frame = row.findChild<QWidget*>("chatLinkPreview");
    ASSERT_NE(frame, nullptr);
    EXPECT_TRUE(frame->isHidden());
    EXPECT_TRUE(frame->findChild<QLabel*>("mutedDescription") != nullptr);
    EXPECT_EQ(frame->findChild<QLabel*>("mutedDescription")->text(), QStringLiteral("example.test"));
}

TEST(ChatMessageRowTest, SetLinkPreviewShowsTitleAndDescriptionAndTheFrame) {
    ChatMessageRow row(sampleMessageWithUrl(), /*showHeader=*/true, /*isOwnMessage=*/false);

    LinkPreviewInfo info;
    info.available = true;
    info.title = "Example Article";
    info.description = "A short summary.";
    row.setLinkPreview(info);

    auto* frame = row.findChild<QWidget*>("chatLinkPreview");
    ASSERT_NE(frame, nullptr);
    // isHidden(), не isVisible() — тест не показывает окно (см. пояснение выше в файле).
    EXPECT_FALSE(frame->isHidden());
    auto* title = row.findChild<QLabel*>("chatLinkPreviewTitle");
    ASSERT_NE(title, nullptr);
    EXPECT_EQ(title->text(), QStringLiteral("Example Article"));
    EXPECT_FALSE(title->isHidden());
    auto* description = row.findChild<QLabel*>("chatLinkPreviewDescription");
    ASSERT_NE(description, nullptr);
    EXPECT_EQ(description->text(), QStringLiteral("A short summary."));
}

TEST(ChatMessageRowTest, SetLinkPreviewWithUnavailableLeavesTheFrameHidden) {
    ChatMessageRow row(sampleMessageWithUrl(), /*showHeader=*/true, /*isOwnMessage=*/false);

    LinkPreviewInfo info;
    info.available = false;
    row.setLinkPreview(info);

    auto* frame = row.findChild<QWidget*>("chatLinkPreview");
    ASSERT_NE(frame, nullptr);
    EXPECT_TRUE(frame->isHidden());
}

TEST(ChatMessageRowTest, SetLinkPreviewOnARowWithoutAUrlIsANoop) {
    ChatMessageRow row(sampleMessage(), /*showHeader=*/true, /*isOwnMessage=*/false);

    LinkPreviewInfo info;
    info.available = true;
    info.title = "Should not appear";
    row.setLinkPreview(info);  // must not crash

    EXPECT_EQ(row.findChild<QWidget*>("chatLinkPreview"), nullptr);
}

TEST(ChatMessageRowTest, SetLinkPreviewImageShowsThePixmap) {
    ChatMessageRow row(sampleMessageWithUrl(), /*showHeader=*/true, /*isOwnMessage=*/false);

    QImage image(8, 8, QImage::Format_ARGB32);
    image.fill(Qt::blue);
    row.setLinkPreviewImage(image);

    auto* imageLabel = row.findChild<QLabel*>("chatLinkPreviewImage");
    ASSERT_NE(imageLabel, nullptr);
    EXPECT_FALSE(imageLabel->isHidden());
    EXPECT_FALSE(imageLabel->pixmap().isNull());
}

TEST(ChatMessageRowTest, SetLinkPreviewImageWithANullImageIsANoop) {
    ChatMessageRow row(sampleMessageWithUrl(), /*showHeader=*/true, /*isOwnMessage=*/false);

    row.setLinkPreviewImage(QImage());  // must not crash

    auto* imageLabel = row.findChild<QLabel*>("chatLinkPreviewImage");
    ASSERT_NE(imageLabel, nullptr);
    EXPECT_TRUE(imageLabel->isHidden());
}

}  // namespace
}  // namespace devicehub
