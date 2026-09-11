#include "ui/ChatView.h"

#include <QColor>
#include <QDate>
#include <QFrame>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSize>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>

#include "ui/ChatMessageGrouping.h"
#include "ui/IconFactory.h"
#include "ui/Theme.h"

namespace devicehub {

namespace {
constexpr int kPlaceholderPageIndex = 0;
constexpr int kChannelPageIndex = 1;
constexpr int kTypingIndicatorHideMs = 3000;
constexpr int kTypingThrottleMs = 2000;
constexpr int kComposerIconButtonSize = 32;
constexpr int kComposerIconGlyphSize = 18;
/// Насколько близко к низу (в пикселях) всё ещё считается "внизу" для
/// stickToBottom_ — небольшой запас, а не требование точного
/// максимального значения, которое округление layout'а может промахнуть
/// на пиксель-другой.
constexpr int kStickToBottomThresholdPx = 4;
/// Максимальная длина фрагмента текста оригинала в цитате-ответе (issue
/// #306) — длиннее обрезается с "…", чтобы длинное цитируемое сообщение
/// не растягивало чужую строку сильнее, чем сам ответ.
constexpr int kReplySnippetMaxChars = 60;

QString truncatedReplySnippet(const QString& body) {
    if (body.size() <= kReplySnippetMaxChars) {
        return body;
    }
    return body.left(kReplySnippetMaxChars) + QStringLiteral("…");
}

/// Линейный перебор в поисках ChatMessageRow, показывающего @p id — не
/// каждый виджет в messagesLayout_ им является (appendSystemLine()
/// тоже добавляет обычные QLabel), отсюда защита через qobject_cast.
/// Списки сообщений достаточно короткие (по одной странице за раз),
/// чтобы не требовалось ничего более изощрённого.
ChatMessageRow* findMessageRow(QVBoxLayout* layout, qint64 id) {
    for (int i = 0; i < layout->count(); ++i) {
        if (auto* row = qobject_cast<ChatMessageRow*>(layout->itemAt(i)->widget()); row != nullptr) {
            if (row->messageId() == id) {
                return row;
            }
        }
    }
    return nullptr;
}
}  // namespace

ChatView::ChatView(QWidget* parent) : QWidget(parent) {
    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);

    stack_ = new QStackedWidget(this);

    auto* placeholderPage = new QWidget(stack_);
    auto* placeholderLayout = new QVBoxLayout(placeholderPage);
    placeholderLayout->setSpacing(ui_theme::kSpacingSm);
    placeholderLayout->addStretch();

    auto* placeholderLabel = new QLabel(tr("Select a channel to start chatting"), placeholderPage);
    placeholderLabel->setObjectName(QStringLiteral("mainContentPlaceholder"));
    placeholderLabel->setAlignment(Qt::AlignCenter);

    auto* placeholderDescription =
        new QLabel(tr("Pick a channel on the left, or create a new one to get the conversation going."), placeholderPage);
    placeholderDescription->setObjectName(QStringLiteral("mutedDescription"));
    placeholderDescription->setAlignment(Qt::AlignCenter);
    placeholderDescription->setWordWrap(true);

    auto* placeholderCreateButton = new QPushButton(tr("Create channel"), placeholderPage);
    placeholderCreateButton->setObjectName(QStringLiteral("placeholderCreateChannelButton"));
    placeholderCreateButton->setProperty("accent", true);
    connect(placeholderCreateButton, &QPushButton::clicked, this, &ChatView::createChannelRequested);

    placeholderLayout->addWidget(placeholderLabel);
    placeholderLayout->addWidget(placeholderDescription);
    placeholderLayout->addWidget(placeholderCreateButton, /*stretch=*/0, Qt::AlignHCenter);
    placeholderLayout->addStretch();

    auto* channelPage = new QWidget(stack_);
    auto* channelLayout = new QVBoxLayout(channelPage);
    channelLayout->setContentsMargins(ui_theme::kSpacingMd, ui_theme::kSpacingMd, ui_theme::kSpacingMd,
                                       ui_theme::kSpacingMd);
    channelLayout->setSpacing(ui_theme::kSpacingSm);

    channelTitleLabel_ = new QLabel(channelPage);
    channelTitleLabel_->setObjectName(QStringLiteral("chatChannelTitle"));
    channelTitleLabel_->setProperty("sectionTitle", true);

    callToggleButton_ = new QPushButton(tr("Call"), channelPage);
    callToggleButton_->setObjectName(QStringLiteral("callToggleButton"));
    connect(callToggleButton_, &QPushButton::clicked, this, &ChatView::callToggleRequested);

    searchButton_ = new QPushButton(tr("Search"), channelPage);
    searchButton_->setObjectName(QStringLiteral("searchButton"));
    connect(searchButton_, &QPushButton::clicked, this, &ChatView::openSearchRequested);

    // Иконка вместо текстовой кнопки (issue #184) — тот же плоский
    // иконочный стиль, что и у композера/FooterBar. ChatView не хранит
    // открыта ли сейчас MemberListPanel (ей не владеет), поэтому кнопка
    // просто сигнализирует клик каждый раз, без переключения своего
    // текста/вида.
    memberListToggleButton_ = new QPushButton(channelPage);
    memberListToggleButton_->setObjectName(QStringLiteral("memberListToggleButton"));
    memberListToggleButton_->setToolTip(tr("Members"));
    memberListToggleButton_->setProperty("flatIconButton", true);
    memberListToggleButton_->setIcon(ui_icons::membersIcon(QColor(ui_theme::kMutedForeground)));
    memberListToggleButton_->setIconSize(QSize(kComposerIconGlyphSize, kComposerIconGlyphSize));
    memberListToggleButton_->setFixedSize(kComposerIconButtonSize, kComposerIconButtonSize);
    connect(memberListToggleButton_, &QPushButton::clicked, this, &ChatView::memberListToggleRequested);

    auto* headerRow = new QHBoxLayout;
    headerRow->setSpacing(ui_theme::kSpacingSm);
    headerRow->addWidget(channelTitleLabel_, /*stretch=*/1);
    headerRow->addWidget(callToggleButton_);
    headerRow->addWidget(searchButton_);
    headerRow->addWidget(memberListToggleButton_);

    loadOlderButton_ = new QPushButton(tr("Load older messages"), channelPage);
    loadOlderButton_->setObjectName(QStringLiteral("loadOlderMessagesButton"));
    loadOlderButton_->setVisible(false);
    connect(loadOlderButton_, &QPushButton::clicked, this, &ChatView::loadOlderMessagesRequested);

    scrollArea_ = new QScrollArea(channelPage);
    scrollArea_->setObjectName(QStringLiteral("chatMessagesScrollArea"));
    scrollArea_->setWidgetResizable(true);
    scrollArea_->setFrameShape(QFrame::NoFrame);
    scrollArea_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    messagesContainer_ = new QWidget(scrollArea_);
    messagesContainer_->setObjectName(QStringLiteral("chatMessagesContainer"));
    messagesLayout_ = new QVBoxLayout(messagesContainer_);
    messagesLayout_->setContentsMargins(0, 0, 0, 0);
    messagesLayout_->setSpacing(ui_theme::kSpacingSm);
    messagesLayout_->addStretch(1);
    scrollArea_->setWidget(messagesContainer_);

    // Удерживаем вид на самом новом сообщении при росте содержимого, но
    // только пока пользователь уже был внизу (stickToBottom_,
    // обновляется ниже по мере прокрутки) — иначе новое живое сообщение
    // или страница истории от prependMessages(), загруженная выше
    // текущего вида, дёргала бы их обратно вниз, пока они читают более
    // старые сообщения.
    connect(scrollArea_->verticalScrollBar(), &QScrollBar::rangeChanged, this, [this](int /*min*/, int max) {
        if (stickToBottom_) {
            scrollArea_->verticalScrollBar()->setValue(max);
        }
    });
    connect(scrollArea_->verticalScrollBar(), &QScrollBar::valueChanged, this, [this](int value) {
        stickToBottom_ = value >= scrollArea_->verticalScrollBar()->maximum() - kStickToBottomThresholdPx;
    });

    typingIndicatorLabel_ = new QLabel(channelPage);
    typingIndicatorLabel_->setObjectName(QStringLiteral("mutedDescription"));
    typingIndicatorLabel_->setVisible(false);

    typingIndicatorHideTimer_ = new QTimer(this);
    typingIndicatorHideTimer_->setSingleShot(true);
    typingIndicatorHideTimer_->setInterval(kTypingIndicatorHideMs);
    connect(typingIndicatorHideTimer_, &QTimer::timeout, this,
            [this]() { typingIndicatorLabel_->setVisible(false); });

    // Ограничивает typingRequested() не чаще одного раза за
    // kTypingThrottleMs, пока пользователь продолжает печатать, вместо
    // испускания сигнала (и отправки WebSocket-фрейма) на каждое
    // нажатие клавиши.
    typingThrottleTimer_ = new QTimer(this);
    typingThrottleTimer_->setSingleShot(true);
    typingThrottleTimer_->setInterval(kTypingThrottleMs);

    // Виден только в режиме редактирования (issue #182 — иконка "Send"
    // теперь не может сама по себе сказать "Update", как раньше умел её
    // текст, см. connectMessageRow()/cancelEditingMessage() ниже).
    editingIndicatorLabel_ = new QLabel(tr("Editing message"), channelPage);
    editingIndicatorLabel_->setObjectName(QStringLiteral("mutedDescription"));
    editingIndicatorLabel_->setVisible(false);

    // Полоса "Replying to ..." (issue #306) — видна только пока есть
    // цель ответа (setReplyTarget()); кнопка "Cancel" рядом с ней просто
    // снимает цель, ничего не отправляя.
    replyBar_ = new QWidget(channelPage);
    replyBar_->setVisible(false);
    auto* replyBarLayout = new QHBoxLayout(replyBar_);
    replyBarLayout->setContentsMargins(0, 0, 0, 0);
    replyBarLayout->setSpacing(ui_theme::kSpacingSm);
    replyBarLabel_ = new QLabel(replyBar_);
    replyBarLabel_->setObjectName(QStringLiteral("chatReplyBarLabel"));
    replyBarLayout->addWidget(replyBarLabel_, /*stretch=*/1);
    auto* replyBarCancelButton = new QPushButton(tr("Cancel"), replyBar_);
    replyBarCancelButton->setObjectName(QStringLiteral("chatReplyBarCancelButton"));
    connect(replyBarCancelButton, &QPushButton::clicked, this, &ChatView::clearReplyTarget);
    replyBarLayout->addWidget(replyBarCancelButton);

    // Композер как единая "таблетка" (issue #182) —
    // messageEdit_/attachButton_/sendButton_ рисуются
    // без собственного фона/рамки (см. Theme.cpp) и сливаются в один
    // скруглённый контейнер вместо трёх раздельных прямоугольных
    // элементов управления в ряд.
    auto* composer = new QWidget(channelPage);
    composer->setObjectName(QStringLiteral("chatComposer"));
    composer->setAttribute(Qt::WA_StyledBackground, true);
    auto* composerLayout = new QHBoxLayout(composer);
    composerLayout->setContentsMargins(ui_theme::kSpacingSm, ui_theme::kSpacingSm, ui_theme::kSpacingSm,
                                        ui_theme::kSpacingSm);
    composerLayout->setSpacing(ui_theme::kSpacingSm);

    messageEdit_ = new QLineEdit(composer);
    messageEdit_->setObjectName(QStringLiteral("chatMessageEdit"));
    messageEdit_->setProperty("composerInput", true);
    messageEdit_->setPlaceholderText(tr("Message"));
    connect(messageEdit_, &QLineEdit::textEdited, this, [this]() {
        if (typingThrottleTimer_->isActive()) {
            return;
        }
        typingThrottleTimer_->start();
        emit typingRequested();
    });

    attachButton_ = new QPushButton(composer);
    attachButton_->setObjectName(QStringLiteral("attachFileButton"));
    attachButton_->setToolTip(tr("Attach"));
    attachButton_->setProperty("flatIconButton", true);
    attachButton_->setIcon(ui_icons::plusIcon(QColor(ui_theme::kMutedForeground)));
    attachButton_->setIconSize(QSize(kComposerIconGlyphSize, kComposerIconGlyphSize));
    attachButton_->setFixedSize(kComposerIconButtonSize, kComposerIconButtonSize);
    connect(attachButton_, &QPushButton::clicked, this, &ChatView::attachFileRequested);

    sendButton_ = new QPushButton(composer);
    sendButton_->setObjectName(QStringLiteral("sendChatMessageButton"));
    sendButton_->setToolTip(tr("Send"));
    sendButton_->setProperty("flatIconButton", true);
    sendButton_->setIcon(ui_icons::sendIcon(QColor(ui_theme::kAccentGradientStart)));
    sendButton_->setIconSize(QSize(kComposerIconGlyphSize, kComposerIconGlyphSize));
    sendButton_->setFixedSize(kComposerIconButtonSize, kComposerIconButtonSize);
    connect(messageEdit_, &QLineEdit::returnPressed, sendButton_, &QPushButton::click);

    composerLayout->addWidget(attachButton_);
    composerLayout->addWidget(messageEdit_, /*stretch=*/1);
    composerLayout->addWidget(sendButton_);

    channelLayout->addLayout(headerRow);
    channelLayout->addWidget(loadOlderButton_, /*stretch=*/0, Qt::AlignHCenter);
    channelLayout->addWidget(scrollArea_, /*stretch=*/1);
    channelLayout->addWidget(typingIndicatorLabel_);
    channelLayout->addWidget(editingIndicatorLabel_);
    channelLayout->addWidget(replyBar_);
    channelLayout->addWidget(composer);

    stack_->insertWidget(kPlaceholderPageIndex, placeholderPage);
    stack_->insertWidget(kChannelPageIndex, channelPage);
    stack_->setCurrentIndex(kPlaceholderPageIndex);

    rootLayout->addWidget(stack_);
}

void ChatView::showPlaceholder() {
    stack_->setCurrentIndex(kPlaceholderPageIndex);
}

void ChatView::showChannel(const QString& channelName) {
    currentChannelName_ = channelName;
    updateChannelTitleLabel();
    stack_->setCurrentIndex(kChannelPageIndex);
    // Индикатор набора текста из предыдущего канала здесь неприменим.
    typingIndicatorHideTimer_->stop();
    typingIndicatorLabel_->setVisible(false);
}

void ChatView::setEncrypted(bool encrypted) {
    encrypted_ = encrypted;
    updateChannelTitleLabel();
    attachButton_->setEnabled(!encrypted);
    attachButton_->setToolTip(encrypted
                                   ? tr("Attachments aren't supported in encrypted channels yet")
                                   : QString());
    searchButton_->setEnabled(!encrypted);
    searchButton_->setToolTip(encrypted ? tr("Search isn't available in encrypted channels") : QString());
}

void ChatView::updateChannelTitleLabel() {
    channelTitleLabel_->setText(encrypted_ ? QStringLiteral("\U0001F512 ") + currentChannelName_
                                            : currentChannelName_);
}

void ChatView::setCurrentUserLogin(const QString& login) {
    currentUserLogin_ = login;
}

ChatMessage ChatView::resolveReplyPreview(const ChatMessage& message) const {
    if (message.replyToMessageId < 0) {
        return message;
    }
    ChatMessage resolved = message;
    if (const auto it = messagesById_.constFind(resolved.replyToMessageId); it != messagesById_.constEnd()) {
        resolved.replyToAuthor = it->author;
        resolved.replyToBodySnippet = truncatedReplySnippet(it->body);
    }
    return resolved;
}

void ChatView::appendMessage(const ChatMessage& message) {
    const ChatMessage resolvedMessage = resolveReplyPreview(message);
    if (!hasLastMessage_ || chat_message_grouping::isDifferentCalendarDay(lastMessage_, resolvedMessage)) {
        messagesLayout_->insertWidget(messagesLayout_->count() - 1, buildDateSeparatorLabel(resolvedMessage.sentAt));
    }
    const bool showHeader =
        !hasLastMessage_ || !chat_message_grouping::shouldGroupWithPrevious(lastMessage_, resolvedMessage);
    const bool isOwnMessage = !currentUserLogin_.isEmpty() && resolvedMessage.author == currentUserLogin_;
    auto* row = new ChatMessageRow(resolvedMessage, showHeader, isOwnMessage, currentUserLogin_, messagesContainer_);
    connectMessageRow(row);
    messagesLayout_->insertWidget(messagesLayout_->count() - 1, row);
    requestPreviewIfImageAttachment(resolvedMessage, row);
    messagesById_.insert(resolvedMessage.id, resolvedMessage);
    lastMessage_ = resolvedMessage;
    hasLastMessage_ = true;
}

void ChatView::prependMessages(const QList<ChatMessage>& messages) {
    if (messages.isEmpty()) {
        return;
    }
    QScrollBar* scrollBar = scrollArea_->verticalScrollBar();
    const int previousMax = scrollBar->maximum();
    const int previousValue = scrollBar->value();

    // Группировка идёт только относительно предыдущего сообщения в этой
    // же пачке — не сравнивается с тем, что уже было самым старым
    // показанным сообщением, так что границы пагинации не дотягиваются
    // до уже отрисованной истории (см. doc-комментарий prependMessages()
    // в ChatView.h). Тот же приём, что и у showHeaderForNext ниже —
    // previousInBatch{} по умолчанию пуст, поэтому isDifferentCalendarDay()
    // для самого первого сообщения пачки вернёт true сама по себе
    // (пустая метка времени не разбирается) без отдельной проверки "это
    // первая итерация?".
    bool showHeaderForNext = true;
    ChatMessage previousInBatch{};
    int insertIndex = 0;
    for (const ChatMessage& message : messages) {
        const ChatMessage resolvedMessage = resolveReplyPreview(message);
        const bool showHeader = showHeaderForNext ||
                                 !chat_message_grouping::shouldGroupWithPrevious(previousInBatch, resolvedMessage);
        if (chat_message_grouping::isDifferentCalendarDay(previousInBatch, resolvedMessage)) {
            messagesLayout_->insertWidget(insertIndex++, buildDateSeparatorLabel(resolvedMessage.sentAt));
        }
        const bool isOwnMessage = !currentUserLogin_.isEmpty() && resolvedMessage.author == currentUserLogin_;
        auto* row = new ChatMessageRow(resolvedMessage, showHeader, isOwnMessage, currentUserLogin_, messagesContainer_);
        // Issue #330: подгруженные через "Load older messages" строки
        // раньше не подключались вообще — Edit/Delete/Download на них
        // молча ничего не делали. Обнаружено при добавлении Reply,
        // которому та же проводка нужна для старых сообщений точно так
        // же, как и для новых; исправлено заодно с остальными тремя.
        connectMessageRow(row);
        messagesLayout_->insertWidget(insertIndex++, row);
        requestPreviewIfImageAttachment(resolvedMessage, row);
        messagesById_.insert(resolvedMessage.id, resolvedMessage);
        previousInBatch = resolvedMessage;
        showHeaderForNext = false;
    }

    // Содержимое только что выросло выше текущей видимой области —
    // rangeChanged не станет заново прижимать вид к низу (stickToBottom_
    // всегда false, когда этот код достижим, поскольку загрузка старой
    // истории случается только после прокрутки вверх), но сырое
    // значение полосы прокрутки всё равно нужно сдвинуть на величину,
    // на которую выросло содержимое, иначе вид как будто дёрнется.
    // Qt не успевает синхронно пересчитать диапазон в рамках этого
    // вызова, поэтому корректировка откладывается на один оборот цикла
    // событий; защищена через QPointer на случай, если к тому моменту
    // view уже уничтожен (например, из-за переключения канала).
    QPointer<QScrollBar> guardedScrollBar(scrollBar);
    QTimer::singleShot(0, this, [guardedScrollBar, previousMax, previousValue]() {
        if (guardedScrollBar.isNull()) {
            return;
        }
        const int addedHeight = guardedScrollBar->maximum() - previousMax;
        if (addedHeight > 0) {
            guardedScrollBar->setValue(previousValue + addedHeight);
        }
    });
}

void ChatView::setLoadOlderVisible(bool visible) {
    loadOlderButton_->setVisible(visible);
}

void ChatView::connectMessageRow(ChatMessageRow* row) {
    connect(row, &ChatMessageRow::editRequested, this, [this](qint64 id, const QString& currentBody) {
        clearReplyTarget();
        editingMessageId_ = id;
        messageEdit_->setText(currentBody);
        messageEdit_->setFocus();
        editingIndicatorLabel_->setVisible(true);
    });
    connect(row, &ChatMessageRow::deleteRequested, this, &ChatView::deleteMessageRequested);
    connect(row, &ChatMessageRow::downloadRequested, this, &ChatView::downloadAttachmentRequested);
    connect(row, &ChatMessageRow::reactionToggleRequested, this, &ChatView::reactionToggleRequested);
    connect(row, &ChatMessageRow::replyRequested, this, &ChatView::setReplyTarget);
}

void ChatView::requestPreviewIfImageAttachment(const ChatMessage& message, ChatMessageRow* row) {
    if (message.attachmentId < 0 || !isImageAttachment(message.attachmentFilename)) {
        return;
    }
    pendingImagePreviewRows_.insert(message.attachmentId, row);
    emit previewAttachmentRequested(message.attachmentId);
}

void ChatView::setAttachmentPreview(qint64 attachmentId, const QImage& image) {
    const QPointer<ChatMessageRow> row = pendingImagePreviewRows_.take(attachmentId);
    if (row.isNull()) {
        return;
    }
    row->setAttachmentPreview(image);
}

QLabel* ChatView::buildDateSeparatorLabel(const QString& sentAt) {
    const QDateTime parsed = chat_message_grouping::parseSentAt(sentAt);
    QString text;
    if (!parsed.isValid()) {
        text = tr("Unknown date");
    } else if (const QDate date = parsed.date(); date == QDate::currentDate()) {
        text = tr("Today");
    } else if (date == QDate::currentDate().addDays(-1)) {
        text = tr("Yesterday");
    } else {
        text = QLocale().toString(date, QStringLiteral("MMMM d, yyyy"));
    }
    auto* label = new QLabel(text, messagesContainer_);
    label->setObjectName(QStringLiteral("chatDateSeparator"));
    label->setProperty("sectionTitle", true);
    label->setAlignment(Qt::AlignCenter);
    return label;
}

void ChatView::updateMessageBody(qint64 id, const QString& newBody) {
    if (ChatMessageRow* row = findMessageRow(messagesLayout_, id); row != nullptr) {
        row->updateBody(newBody);
    }
    // Кэш сообщений для резолва цитат-ответов (issue #306) должен
    // отражать редактирование — иначе будущий ответ на это сообщение
    // процитировал бы уже неактуальный текст.
    if (const auto it = messagesById_.find(id); it != messagesById_.end()) {
        it->body = newBody;
    }
}

void ChatView::updateReactions(qint64 id, const QString& emoji, const QStringList& logins) {
    if (ChatMessageRow* row = findMessageRow(messagesLayout_, id); row != nullptr) {
        row->applyReactionChange(emoji, logins);
    }
}

bool ChatView::scrollToMessage(qint64 id) {
    ChatMessageRow* row = findMessageRow(messagesLayout_, id);
    if (row == nullptr) {
        return false;
    }
    scrollArea_->ensureWidgetVisible(row);
    return true;
}

void ChatView::removeMessage(qint64 id) {
    if (id == editingMessageId_) {
        // Сообщение, которое редактировалось, только что удалили прямо
        // из-под поля отправки — выходим из режима редактирования, а не
        // позволяем "Update" отправить edit_message для id, которого
        // больше не существует.
        cancelEditingMessage();
    }
    if (id == replyTargetId_) {
        // То же самое для цели ответа (issue #306) — не отправлять
        // reply_to_message_id, указывающий на только что удалённое
        // сообщение.
        clearReplyTarget();
    }
    delete findMessageRow(messagesLayout_, id);
    messagesById_.remove(id);
}

void ChatView::cancelEditingMessage() {
    editingMessageId_ = -1;
    messageEdit_->clear();
    editingIndicatorLabel_->setVisible(false);
}

void ChatView::setReplyTarget(qint64 id) {
    const auto it = messagesById_.constFind(id);
    if (it == messagesById_.constEnd()) {
        return;
    }
    cancelEditingMessage();
    replyTargetId_ = id;
    replyBarLabel_->setText(tr("Replying to %1: %2").arg(it->author, truncatedReplySnippet(it->body)));
    replyBar_->setVisible(true);
    messageEdit_->setFocus();
}

void ChatView::clearReplyTarget() {
    replyTargetId_ = -1;
    replyBar_->setVisible(false);
}

qint64 ChatView::consumeReplyTarget() {
    const qint64 id = replyTargetId_;
    clearReplyTarget();
    return id;
}

void ChatView::appendSystemLine(const QString& text) {
    auto* label = new QLabel(text, messagesContainer_);
    label->setObjectName(QStringLiteral("mutedDescription"));
    label->setAlignment(Qt::AlignCenter);
    label->setWordWrap(true);
    messagesLayout_->insertWidget(messagesLayout_->count() - 1, label);
    hasLastMessage_ = false;
}

void ChatView::setCallState(bool inCall) {
    callToggleButton_->setText(inCall ? tr("Leave call") : tr("Call"));
}

void ChatView::showTypingUser(const QString& login) {
    typingIndicatorLabel_->setText(tr("%1 is typing…").arg(login));
    typingIndicatorLabel_->setVisible(true);
    typingIndicatorHideTimer_->start();
}

void ChatView::clearLog() {
    while (messagesLayout_->count() > 1) {
        QLayoutItem* item = messagesLayout_->takeAt(0);
        delete item->widget();
        delete item;
    }
    hasLastMessage_ = false;
    setLoadOlderVisible(false);
    // То, что редактировалось, принадлежало каналу, который только что
    // очистили — id из него потеряет смысл (или, хуже, совпадёт с id
    // из другого канала), как только загрузятся сообщения нового
    // канала.
    cancelEditingMessage();
    // Цель ответа (issue #306) принадлежала тому же старому каналу — по
    // той же причине, что и cancelEditingMessage() выше.
    clearReplyTarget();
    // Строки, на которые эти записи ссылались, только что удалены выше
    // (QPointer сам обнулился бы и без этого) — очищаем сразу, а не
    // ждём, пока setAttachmentPreview() найдёт их null одну за другой.
    pendingImagePreviewRows_.clear();
    messagesById_.clear();
}

}  // namespace devicehub
