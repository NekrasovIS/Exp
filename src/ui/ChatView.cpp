#include "ui/ChatView.h"

#include <QColor>
#include <QFrame>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QPixmap>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSize>
#include <QSizePolicy>
#include <QSplitter>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QVideoWidget>

#include <utility>

#include "ui/ChatMessageGrouping.h"
#include "ui/IconFactory.h"
#include "ui/Theme.h"

namespace devicehub {

namespace {
constexpr int kPlaceholderPageIndex = 0;
constexpr int kChannelPageIndex = 1;
constexpr int kVideoTileSize = 160;
constexpr int kTypingIndicatorHideMs = 3000;
constexpr int kTypingThrottleMs = 2000;
constexpr int kComposerIconButtonSize = 32;
constexpr int kComposerIconGlyphSize = 18;
/// Насколько близко к низу (в пикселях) всё ещё считается "внизу" для
/// stickToBottom_ — небольшой запас, а не требование точного
/// максимального значения, которое округление layout'а может промахнуть
/// на пиксель-другой.
constexpr int kStickToBottomThresholdPx = 4;

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

    muteToggleButton_ = new QPushButton(tr("Mute"), channelPage);
    muteToggleButton_->setObjectName(QStringLiteral("muteToggleButton"));
    muteToggleButton_->setEnabled(false);
    connect(muteToggleButton_, &QPushButton::clicked, this, &ChatView::muteToggleRequested);

    videoToggleButton_ = new QPushButton(tr("Enable Video"), channelPage);
    videoToggleButton_->setObjectName(QStringLiteral("videoToggleButton"));
    videoToggleButton_->setEnabled(false);
    connect(videoToggleButton_, &QPushButton::clicked, this, &ChatView::videoToggleRequested);

    screenShareToggleButton_ = new QPushButton(tr("Share Screen"), channelPage);
    screenShareToggleButton_->setObjectName(QStringLiteral("screenShareToggleButton"));
    screenShareToggleButton_->setEnabled(false);
    connect(screenShareToggleButton_, &QPushButton::clicked, this, &ChatView::screenShareToggleRequested);

    searchButton_ = new QPushButton(tr("Search"), channelPage);
    searchButton_->setObjectName(QStringLiteral("searchButton"));
    connect(searchButton_, &QPushButton::clicked, this, &ChatView::openSearchRequested);

    // Видна только во время звонка (issue #153) — позволяет области
    // видео занять окно, не теряя доступ к чату насовсем.
    toggleChatVisibilityButton_ = new QPushButton(tr("Hide Chat"), channelPage);
    toggleChatVisibilityButton_->setObjectName(QStringLiteral("toggleChatVisibilityButton"));
    toggleChatVisibilityButton_->setVisible(false);
    connect(toggleChatVisibilityButton_, &QPushButton::clicked, this, &ChatView::onToggleChatVisibilityClicked);

    auto* headerRow = new QHBoxLayout;
    headerRow->setSpacing(ui_theme::kSpacingSm);
    headerRow->addWidget(channelTitleLabel_, /*stretch=*/1);
    headerRow->addWidget(callToggleButton_);
    headerRow->addWidget(muteToggleButton_);
    headerRow->addWidget(videoToggleButton_);
    headerRow->addWidget(screenShareToggleButton_);
    headerRow->addWidget(toggleChatVisibilityButton_);
    headerRow->addWidget(searchButton_);

    callParticipantsLabel_ = new QLabel(channelPage);
    callParticipantsLabel_->setObjectName(QStringLiteral("mutedDescription"));
    callParticipantsLabel_->setWordWrap(true);
    callParticipantsLabel_->setVisible(false);

    // Локальное превью + по одной плитке на каждого удалённого
    // участника, сейчас отправляющего видео (issue #91) — скрывается,
    // когда показывать нечего (нет активного звонка или видео ещё не
    // включено), тот же приём show/hide, что и у callParticipantsLabel_
    // выше.
    videoStrip_ = new QWidget(channelPage);
    videoStripLayout_ = new QHBoxLayout(videoStrip_);
    videoStripLayout_->setContentsMargins(0, 0, 0, 0);
    videoStripLayout_->setSpacing(ui_theme::kSpacingSm);
    videoStripLayout_->addStretch(1);

    localVideoWidget_ = new QVideoWidget(videoStrip_);
    localVideoWidget_->setObjectName(QStringLiteral("localVideoWidget"));
    // Минимальный, а не фиксированный размер (issue #153) —
    // chatSplitter_ отдаёт videoStrip_ большую часть окна во время
    // звонка, и Expanding-политика размера позволяет плитке реально
    // вырасти в это пространство, а не оставаться приколоченной к
    // kVideoTileSize независимо от того, сколько места ей выделено.
    localVideoWidget_->setMinimumSize(kVideoTileSize, kVideoTileSize);
    localVideoWidget_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    videoStripLayout_->insertWidget(0, localVideoWidget_);
    videoStrip_->setVisible(false);

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

    composerLayout->addWidget(attachButton_);
    composerLayout->addWidget(messageEdit_, /*stretch=*/1);
    composerLayout->addWidget(sendButton_);

    // История сообщений + строка ввода, сгруппированы в один виджет,
    // чтобы быть одним дочерним элементом QSplitter вместе с videoStrip_
    // (issue #153) — во время звонка область видео получает большую
    // часть пространства, а эту панель можно свернуть через
    // toggleChatVisibilityButton_ вместо того, чтобы она всегда занимала
    // фиксированную долю окна.
    chatPanel_ = new QWidget(channelPage);
    auto* chatPanelLayout = new QVBoxLayout(chatPanel_);
    chatPanelLayout->setContentsMargins(0, 0, 0, 0);
    chatPanelLayout->setSpacing(ui_theme::kSpacingSm);
    chatPanelLayout->addWidget(loadOlderButton_, /*stretch=*/0, Qt::AlignHCenter);
    chatPanelLayout->addWidget(scrollArea_, /*stretch=*/1);
    chatPanelLayout->addWidget(typingIndicatorLabel_);
    chatPanelLayout->addWidget(editingIndicatorLabel_);
    chatPanelLayout->addWidget(composer);

    chatSplitter_ = new QSplitter(Qt::Vertical, channelPage);
    chatSplitter_->setObjectName(QStringLiteral("chatSplitter"));
    chatSplitter_->setChildrenCollapsible(true);
    chatSplitter_->addWidget(videoStrip_);
    chatSplitter_->addWidget(chatPanel_);
    // Вне звонка videoStrip_ скрыт (см. updateLocalVideoVisibility()), а
    // chatPanel_ занимает всю область — так же, как и до issue #153;
    // setCallState() пересчитывает разделение, как только звонок
    // реально начинается.
    chatSplitter_->setSizes({0, 1});

    channelLayout->addLayout(headerRow);
    channelLayout->addWidget(callParticipantsLabel_);
    channelLayout->addWidget(chatSplitter_, /*stretch=*/1);

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

void ChatView::appendMessage(const ChatMessage& message) {
    const bool showHeader = !hasLastMessage_ || !chat_message_grouping::shouldGroupWithPrevious(lastMessage_, message);
    const bool isOwnMessage = !currentUserLogin_.isEmpty() && message.author == currentUserLogin_;
    auto* row = new ChatMessageRow(message, showHeader, isOwnMessage, messagesContainer_);
    connectMessageRow(row);
    messagesLayout_->insertWidget(messagesLayout_->count() - 1, row);
    lastMessage_ = message;
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
    // в ChatView.h).
    bool showHeaderForNext = true;
    ChatMessage previousInBatch{};
    int insertIndex = 0;
    for (const ChatMessage& message : messages) {
        const bool showHeader =
            showHeaderForNext || !chat_message_grouping::shouldGroupWithPrevious(previousInBatch, message);
        const bool isOwnMessage = !currentUserLogin_.isEmpty() && message.author == currentUserLogin_;
        auto* row = new ChatMessageRow(message, showHeader, isOwnMessage, messagesContainer_);
        messagesLayout_->insertWidget(insertIndex++, row);
        previousInBatch = message;
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
        editingMessageId_ = id;
        messageEdit_->setText(currentBody);
        messageEdit_->setFocus();
        editingIndicatorLabel_->setVisible(true);
    });
    connect(row, &ChatMessageRow::deleteRequested, this, &ChatView::deleteMessageRequested);
    connect(row, &ChatMessageRow::downloadRequested, this, &ChatView::downloadAttachmentRequested);
}

void ChatView::updateMessageBody(qint64 id, const QString& newBody) {
    if (ChatMessageRow* row = findMessageRow(messagesLayout_, id); row != nullptr) {
        row->updateBody(newBody);
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
    delete findMessageRow(messagesLayout_, id);
}

void ChatView::cancelEditingMessage() {
    editingMessageId_ = -1;
    messageEdit_->clear();
    editingIndicatorLabel_->setVisible(false);
}

void ChatView::appendSystemLine(const QString& text) {
    auto* label = new QLabel(text, messagesContainer_);
    label->setObjectName(QStringLiteral("mutedDescription"));
    label->setAlignment(Qt::AlignCenter);
    label->setWordWrap(true);
    messagesLayout_->insertWidget(messagesLayout_->count() - 1, label);
    hasLastMessage_ = false;
}

void ChatView::setCallState(bool inCall, bool muted) {
    callToggleButton_->setText(inCall ? tr("Leave call") : tr("Call"));
    muteToggleButton_->setEnabled(inCall);
    muteToggleButton_->setText(muted ? tr("Unmute") : tr("Mute"));
    videoToggleButton_->setEnabled(inCall);
    screenShareToggleButton_->setEnabled(inCall);
    toggleChatVisibilityButton_->setVisible(inCall);
    if (inCall) {
        // Отдаём области видео большую часть окна вместо деления 50/50
        // с чатом (issue #153) — setSizes() нужно только соотношение,
        // Qt сам масштабирует его к реальному размеру сплиттера в
        // пикселях.
        chatPanelCollapsed_ = false;
        toggleChatVisibilityButton_->setText(tr("Hide Chat"));
        chatSplitter_->setSizes({3, 2});
    } else {
        callParticipantsLabel_->setVisible(false);
        // Видео/демонстрация экрана не могут пережить звонок, которому
        // принадлежат — сбрасываем оба здесь, чтобы каждое место
        // вызова, связанное с выходом/переключением канала, получало
        // это бесплатно, а не требовало отдельного вызова очистки.
        setVideoEnabled(false);
        setScreenShareEnabled(false);
        for (QLabel* tile : std::as_const(remoteVideoTiles_)) {
            delete tile;
        }
        remoteVideoTiles_.clear();
        // Возврат к обычной, довызывной раскладке: чат получает всю
        // область (videoStrip_ скрыт вызовами setVideoEnabled(false)/
        // setScreenShareEnabled(false) выше, так что не занимает места
        // независимо от соотношения, но 0/1 сохраняет состояние
        // chatSplitter_ согласованным с тем, каким оно было до первого
        // звонка).
        chatPanelCollapsed_ = false;
        chatSplitter_->setSizes({0, 1});
    }
}

void ChatView::onToggleChatVisibilityClicked() {
    chatPanelCollapsed_ = !chatPanelCollapsed_;
    toggleChatVisibilityButton_->setText(chatPanelCollapsed_ ? tr("Show Chat") : tr("Hide Chat"));
    chatSplitter_->setSizes(chatPanelCollapsed_ ? QList<int>{1, 0} : QList<int>{3, 2});
}

void ChatView::setCallParticipants(const QStringList& participants) {
    if (participants.isEmpty()) {
        callParticipantsLabel_->setVisible(false);
        return;
    }
    callParticipantsLabel_->setText(tr("In call: %1").arg(participants.join(QStringLiteral(", "))));
    callParticipantsLabel_->setVisible(true);
}

void ChatView::setVideoEnabled(bool enabled) {
    videoToggleButton_->setText(enabled ? tr("Disable Video") : tr("Enable Video"));
    videoActive_ = enabled;
    updateLocalVideoVisibility();
}

void ChatView::setScreenShareEnabled(bool enabled) {
    screenShareToggleButton_->setText(enabled ? tr("Stop Sharing") : tr("Share Screen"));
    screenShareActive_ = enabled;
    updateLocalVideoVisibility();
}

void ChatView::updateLocalVideoVisibility() {
    // Камера и демонстрация экрана в CallManager взаимоисключающие, но
    // MainWindow вызывает оба setVideoEnabled()/setScreenShareEnabled()
    // после каждого переключения (какой бы из них ни стал true, другой
    // становится false) — отслеживание обоих флагов здесь, а не
    // доверие тому, какой сеттер вызывался последним, сохраняет
    // корректность видимости локального превью независимо от порядка
    // вызовов.
    const bool anyActive = videoActive_ || screenShareActive_;
    localVideoWidget_->setVisible(anyActive);
    videoStrip_->setVisible(anyActive || !remoteVideoTiles_.isEmpty());
}

void ChatView::showRemoteVideoFrame(const QString& peerLogin, const QImage& frame) {
    QLabel* tile = remoteVideoTiles_.value(peerLogin, nullptr);
    if (tile == nullptr) {
        tile = new QLabel(videoStrip_);
        tile->setObjectName(QStringLiteral("remoteVideoTile"));
        // Минимальный, а не фиксированный размер — та же логика, что и
        // у localVideoWidget_ выше (issue #153).
        tile->setMinimumSize(kVideoTileSize, kVideoTileSize);
        tile->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        tile->setScaledContents(true);
        videoStripLayout_->addWidget(tile);
        remoteVideoTiles_.insert(peerLogin, tile);
    }
    tile->setPixmap(QPixmap::fromImage(frame));
    videoStrip_->setVisible(true);
}

void ChatView::removeRemoteVideo(const QString& peerLogin) {
    QLabel* tile = remoteVideoTiles_.take(peerLogin);
    if (tile == nullptr) {
        return;
    }
    delete tile;
    if (remoteVideoTiles_.isEmpty() && !localVideoWidget_->isVisible()) {
        videoStrip_->setVisible(false);
    }
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
}

}  // namespace devicehub
