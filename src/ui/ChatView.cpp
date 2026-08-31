#include "ui/ChatView.h"

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
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QVideoWidget>

#include <utility>

#include "ui/ChatMessageGrouping.h"
#include "ui/Theme.h"

namespace devicehub {

namespace {
constexpr int kPlaceholderPageIndex = 0;
constexpr int kChannelPageIndex = 1;
constexpr int kVideoTileSize = 160;
constexpr int kTypingIndicatorHideMs = 3000;
constexpr int kTypingThrottleMs = 2000;
/// How close to the bottom (in px) still counts as "at the bottom" for
/// stickToBottom_ — a hair of slack rather than requiring the exact max
/// value, which layout rounding can miss by a pixel or two.
constexpr int kStickToBottomThresholdPx = 4;

/// Linear scan for the ChatMessageRow showing @p id — not every widget
/// in messagesLayout_ is one (appendSystemLine() adds plain QLabels
/// too), hence the qobject_cast guard. Message lists are short enough
/// (one page at a time) that this doesn't need to be anything fancier.
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

    searchButton_ = new QPushButton(tr("Search"), channelPage);
    searchButton_->setObjectName(QStringLiteral("searchButton"));
    connect(searchButton_, &QPushButton::clicked, this, &ChatView::openSearchRequested);

    auto* headerRow = new QHBoxLayout;
    headerRow->setSpacing(ui_theme::kSpacingSm);
    headerRow->addWidget(channelTitleLabel_, /*stretch=*/1);
    headerRow->addWidget(callToggleButton_);
    headerRow->addWidget(muteToggleButton_);
    headerRow->addWidget(videoToggleButton_);
    headerRow->addWidget(searchButton_);

    callParticipantsLabel_ = new QLabel(channelPage);
    callParticipantsLabel_->setObjectName(QStringLiteral("mutedDescription"));
    callParticipantsLabel_->setWordWrap(true);
    callParticipantsLabel_->setVisible(false);

    // Local preview + one tile per remote participant currently sending
    // video (issue #91) — hidden whenever there's nothing to show (no
    // active call, or video not yet enabled), same show/hide idiom as
    // callParticipantsLabel_ above.
    videoStrip_ = new QWidget(channelPage);
    videoStripLayout_ = new QHBoxLayout(videoStrip_);
    videoStripLayout_->setContentsMargins(0, 0, 0, 0);
    videoStripLayout_->setSpacing(ui_theme::kSpacingSm);
    videoStripLayout_->addStretch(1);

    localVideoWidget_ = new QVideoWidget(videoStrip_);
    localVideoWidget_->setObjectName(QStringLiteral("localVideoWidget"));
    localVideoWidget_->setFixedSize(kVideoTileSize, kVideoTileSize);
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

    // Keep the view pinned to the newest message whenever the content
    // grows, but only while the user was already at the bottom
    // (stickToBottom_, updated below as they scroll) — otherwise a new
    // live message, or a prependMessages() history page loaded above
    // the current view, would yank them back down while they're
    // reading older messages.
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

    // Throttles typingRequested() to at most once per kTypingThrottleMs
    // while the user keeps typing, rather than emitting (and sending a
    // WebSocket frame) on every single keystroke.
    typingThrottleTimer_ = new QTimer(this);
    typingThrottleTimer_->setSingleShot(true);
    typingThrottleTimer_->setInterval(kTypingThrottleMs);

    auto* sendRow = new QHBoxLayout;
    sendRow->setSpacing(ui_theme::kSpacingSm);
    messageEdit_ = new QLineEdit(channelPage);
    messageEdit_->setObjectName(QStringLiteral("chatMessageEdit"));
    messageEdit_->setPlaceholderText(tr("Message"));
    connect(messageEdit_, &QLineEdit::textEdited, this, [this]() {
        if (typingThrottleTimer_->isActive()) {
            return;
        }
        typingThrottleTimer_->start();
        emit typingRequested();
    });

    sendButton_ = new QPushButton(tr("Send"), channelPage);
    sendButton_->setObjectName(QStringLiteral("sendChatMessageButton"));
    sendButton_->setProperty("accent", true);

    sendRow->addWidget(messageEdit_, /*stretch=*/1);
    sendRow->addWidget(sendButton_);

    channelLayout->addLayout(headerRow);
    channelLayout->addWidget(callParticipantsLabel_);
    channelLayout->addWidget(videoStrip_);
    channelLayout->addWidget(loadOlderButton_, /*stretch=*/0, Qt::AlignHCenter);
    channelLayout->addWidget(scrollArea_, /*stretch=*/1);
    channelLayout->addWidget(typingIndicatorLabel_);
    channelLayout->addLayout(sendRow);

    stack_->insertWidget(kPlaceholderPageIndex, placeholderPage);
    stack_->insertWidget(kChannelPageIndex, channelPage);
    stack_->setCurrentIndex(kPlaceholderPageIndex);

    rootLayout->addWidget(stack_);
}

void ChatView::showPlaceholder() {
    stack_->setCurrentIndex(kPlaceholderPageIndex);
}

void ChatView::showChannel(const QString& channelName) {
    channelTitleLabel_->setText(channelName);
    stack_->setCurrentIndex(kChannelPageIndex);
    // A typing indicator from the previous channel doesn't apply here.
    typingIndicatorHideTimer_->stop();
    typingIndicatorLabel_->setVisible(false);
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

    // Grouped against the previous message within this same batch only
    // — not compared to whatever was already the oldest message shown,
    // so pagination boundaries don't reach back into already-rendered
    // history (see prependMessages()'s doc comment in ChatView.h).
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

    // Content just grew above the current viewport — rangeChanged won't
    // re-pin to the bottom (stickToBottom_ is false whenever this is
    // reachable, since loading older history only ever happens after
    // scrolling up), but the raw scrollbar value still needs shifting
    // by however much taller the content got, or the view would appear
    // to jump. Qt hasn't finished recomputing the range synchronously
    // within this call, so the adjustment is deferred one event-loop
    // turn; guarded via QPointer in case the view's gone by then (e.g.
    // a channel switch tore it down).
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
        sendButton_->setText(tr("Update"));
    });
    connect(row, &ChatMessageRow::deleteRequested, this, &ChatView::deleteMessageRequested);
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
        // The message being edited just got deleted out from under the
        // send box — leave edit mode rather than letting "Update" send
        // an edit_message for an id that no longer exists.
        cancelEditingMessage();
    }
    delete findMessageRow(messagesLayout_, id);
}

void ChatView::cancelEditingMessage() {
    editingMessageId_ = -1;
    messageEdit_->clear();
    sendButton_->setText(tr("Send"));
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
    if (!inCall) {
        callParticipantsLabel_->setVisible(false);
        // Video can't outlive the call it belongs to — reset it here so
        // every existing leave/channel-switch call site gets this for
        // free instead of needing its own cleanup call.
        setVideoEnabled(false);
        for (QLabel* tile : std::as_const(remoteVideoTiles_)) {
            delete tile;
        }
        remoteVideoTiles_.clear();
    }
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
    localVideoWidget_->setVisible(enabled);
    videoStrip_->setVisible(enabled || !remoteVideoTiles_.isEmpty());
}

void ChatView::showRemoteVideoFrame(const QString& peerLogin, const QImage& frame) {
    QLabel* tile = remoteVideoTiles_.value(peerLogin, nullptr);
    if (tile == nullptr) {
        tile = new QLabel(videoStrip_);
        tile->setObjectName(QStringLiteral("remoteVideoTile"));
        tile->setFixedSize(kVideoTileSize, kVideoTileSize);
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
    // Whatever was being edited belonged to the channel that just got
    // cleared — an id from it would be meaningless (or, worse, collide
    // with some other channel's id) once a new channel's messages load.
    cancelEditingMessage();
}

}  // namespace devicehub
