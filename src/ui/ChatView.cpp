#include "ui/ChatView.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QStackedWidget>
#include <QVBoxLayout>

#include "ui/ChatMessageGrouping.h"
#include "ui/Theme.h"

namespace devicehub {

namespace {
constexpr int kPlaceholderPageIndex = 0;
constexpr int kChannelPageIndex = 1;

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

    auto* headerRow = new QHBoxLayout;
    headerRow->setSpacing(ui_theme::kSpacingSm);
    headerRow->addWidget(channelTitleLabel_, /*stretch=*/1);
    headerRow->addWidget(callToggleButton_);
    headerRow->addWidget(muteToggleButton_);

    callParticipantsLabel_ = new QLabel(channelPage);
    callParticipantsLabel_->setObjectName(QStringLiteral("mutedDescription"));
    callParticipantsLabel_->setWordWrap(true);
    callParticipantsLabel_->setVisible(false);

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
    connectMessageRow(row);
    messagesLayout_->insertWidget(messagesLayout_->count() - 1, row);
    lastMessage_ = message;
    hasLastMessage_ = true;
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
    // Whatever was being edited belonged to the channel that just got
    // cleared — an id from it would be meaningless (or, worse, collide
    // with some other channel's id) once a new channel's messages load.
    cancelEditingMessage();
}

}  // namespace devicehub
