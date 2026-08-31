#include "ui/ChatView.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QStackedWidget>
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

    auto* headerRow = new QHBoxLayout;
    headerRow->setSpacing(ui_theme::kSpacingSm);
    headerRow->addWidget(channelTitleLabel_, /*stretch=*/1);
    headerRow->addWidget(callToggleButton_);
    headerRow->addWidget(muteToggleButton_);
    headerRow->addWidget(videoToggleButton_);

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
    // grows — simpler than tracking the user's scroll position, at the
    // cost of jumping to the bottom even if they'd scrolled up to read
    // older messages.
    connect(scrollArea_->verticalScrollBar(), &QScrollBar::rangeChanged, this,
            [this](int /*min*/, int max) { scrollArea_->verticalScrollBar()->setValue(max); });

    auto* sendRow = new QHBoxLayout;
    sendRow->setSpacing(ui_theme::kSpacingSm);
    messageEdit_ = new QLineEdit(channelPage);
    messageEdit_->setObjectName(QStringLiteral("chatMessageEdit"));
    messageEdit_->setPlaceholderText(tr("Message"));

    sendButton_ = new QPushButton(tr("Send"), channelPage);
    sendButton_->setObjectName(QStringLiteral("sendChatMessageButton"));
    sendButton_->setProperty("accent", true);

    sendRow->addWidget(messageEdit_, /*stretch=*/1);
    sendRow->addWidget(sendButton_);

    channelLayout->addLayout(headerRow);
    channelLayout->addWidget(callParticipantsLabel_);
    channelLayout->addWidget(videoStrip_);
    channelLayout->addWidget(scrollArea_, /*stretch=*/1);
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
}

void ChatView::setCurrentUserLogin(const QString& login) {
    currentUserLogin_ = login;
}

void ChatView::appendMessage(const ChatMessage& message) {
    const bool showHeader = !hasLastMessage_ || !chat_message_grouping::shouldGroupWithPrevious(lastMessage_, message);
    const bool isOwnMessage = !currentUserLogin_.isEmpty() && message.author == currentUserLogin_;
    auto* row = new ChatMessageRow(message, showHeader, isOwnMessage, messagesContainer_);
    messagesLayout_->insertWidget(messagesLayout_->count() - 1, row);
    lastMessage_ = message;
    hasLastMessage_ = true;
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

void ChatView::clearLog() {
    while (messagesLayout_->count() > 1) {
        QLayoutItem* item = messagesLayout_->takeAt(0);
        delete item->widget();
        delete item;
    }
    hasLastMessage_ = false;
}

}  // namespace devicehub
