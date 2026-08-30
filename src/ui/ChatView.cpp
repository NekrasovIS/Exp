#include "ui/ChatView.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>

#include "ui/ChatMessageGrouping.h"
#include "ui/Theme.h"

namespace devicehub {

namespace {
constexpr int kPlaceholderPageIndex = 0;
constexpr int kChannelPageIndex = 1;
/// How close to the bottom (in px) still counts as "at the bottom" for
/// stickToBottom_ — a hair of slack rather than requiring the exact max
/// value, which layout rounding can miss by a pixel or two.
constexpr int kStickToBottomThresholdPx = 4;
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

    auto* headerRow = new QHBoxLayout;
    headerRow->setSpacing(ui_theme::kSpacingSm);
    headerRow->addWidget(channelTitleLabel_, /*stretch=*/1);
    headerRow->addWidget(callToggleButton_);
    headerRow->addWidget(muteToggleButton_);

    callParticipantsLabel_ = new QLabel(channelPage);
    callParticipantsLabel_->setObjectName(QStringLiteral("mutedDescription"));
    callParticipantsLabel_->setWordWrap(true);
    callParticipantsLabel_->setVisible(false);

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
    channelLayout->addWidget(loadOlderButton_, /*stretch=*/0, Qt::AlignHCenter);
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
    if (!inCall) {
        callParticipantsLabel_->setVisible(false);
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

void ChatView::clearLog() {
    while (messagesLayout_->count() > 1) {
        QLayoutItem* item = messagesLayout_->takeAt(0);
        delete item->widget();
        delete item;
    }
    hasLastMessage_ = false;
    setLoadOlderVisible(false);
}

}  // namespace devicehub
