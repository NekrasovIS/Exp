#include "ui/DirectMessageView.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

#include "ui/Theme.h"

namespace devicehub {

namespace {
// Same constants as ChatView.cpp's own kTypingIndicatorHideMs/
// kTypingThrottleMs (issue #313) — kept as a separate copy here rather
// than a shared header: two unrelated widgets independently choosing
// the same UX timing, not a value that must stay in lockstep.
constexpr int kTypingIndicatorHideMs = 3000;
constexpr int kTypingThrottleMs = 2000;
}  // namespace

DirectMessageView::DirectMessageView(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_StyledBackground, true);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(ui_theme::kSpacingMd, ui_theme::kSpacingMd, ui_theme::kSpacingMd,
                                ui_theme::kSpacingMd);
    layout->setSpacing(ui_theme::kSpacingSm);

    titleLabel_ = new QLabel(tr("Select a friend to start a conversation"), this);
    titleLabel_->setObjectName(QStringLiteral("dmThreadTitleLabel"));
    titleLabel_->setProperty("sectionTitle", true);

    messagesList_ = new QListWidget(this);
    messagesList_->setObjectName(QStringLiteral("dmMessagesList"));
    messagesList_->setFrameShape(QFrame::NoFrame);
    messagesList_->setWordWrap(true);

    typingIndicatorLabel_ = new QLabel(this);
    typingIndicatorLabel_->setObjectName(QStringLiteral("mutedDescription"));
    typingIndicatorLabel_->setVisible(false);

    typingIndicatorHideTimer_ = new QTimer(this);
    typingIndicatorHideTimer_->setSingleShot(true);
    typingIndicatorHideTimer_->setInterval(kTypingIndicatorHideMs);
    connect(typingIndicatorHideTimer_, &QTimer::timeout, this,
            [this]() { typingIndicatorLabel_->setVisible(false); });

    // Same throttle-then-emit shape as ChatView's own messageEdit_
    // wiring — see its doc comment on typingRequested().
    typingThrottleTimer_ = new QTimer(this);
    typingThrottleTimer_->setSingleShot(true);
    typingThrottleTimer_->setInterval(kTypingThrottleMs);

    auto* sendRow = new QHBoxLayout;
    sendRow->setSpacing(ui_theme::kSpacingSm);
    messageEdit_ = new QLineEdit(this);
    messageEdit_->setObjectName(QStringLiteral("dmMessageEdit"));
    messageEdit_->setPlaceholderText(tr("Message"));
    messageEdit_->setEnabled(false);
    connect(messageEdit_, &QLineEdit::returnPressed, this, &DirectMessageView::onSendClicked);
    connect(messageEdit_, &QLineEdit::textEdited, this, [this]() {
        if (typingThrottleTimer_->isActive()) {
            return;
        }
        typingThrottleTimer_->start();
        emit typingRequested();
    });

    sendButton_ = new QPushButton(tr("Send"), this);
    sendButton_->setObjectName(QStringLiteral("sendDmButton"));
    sendButton_->setProperty("accent", true);
    sendButton_->setEnabled(false);
    connect(sendButton_, &QPushButton::clicked, this, &DirectMessageView::onSendClicked);

    sendRow->addWidget(messageEdit_, /*stretch=*/1);
    sendRow->addWidget(sendButton_);

    layout->addWidget(titleLabel_);
    layout->addWidget(messagesList_, /*stretch=*/1);
    layout->addWidget(typingIndicatorLabel_);
    layout->addLayout(sendRow);
}

void DirectMessageView::showPlaceholder() {
    titleLabel_->setText(tr("Select a friend to start a conversation"));
    messagesList_->clear();
    messageEdit_->clear();
    messageEdit_->setEnabled(false);
    sendButton_->setEnabled(false);
    typingIndicatorHideTimer_->stop();
    typingIndicatorLabel_->setVisible(false);
}

void DirectMessageView::showThread(const QString& otherLogin) {
    titleLabel_->setText(otherLogin);
    messagesList_->clear();
    messageEdit_->setEnabled(true);
    sendButton_->setEnabled(true);
    // Индикатор набора текста из предыдущего диалога здесь неприменим
    // — тот же сброс, что и ChatView::showChannel() при смене канала.
    typingIndicatorHideTimer_->stop();
    typingIndicatorLabel_->setVisible(false);
}

void DirectMessageView::showTypingUser(const QString& login) {
    typingIndicatorLabel_->setText(tr("%1 is typing…").arg(login));
    typingIndicatorLabel_->setVisible(true);
    typingIndicatorHideTimer_->start();
}

void DirectMessageView::setMessages(const QList<DirectMessageInfo>& messages) {
    messagesList_->clear();
    for (const DirectMessageInfo& message : messages) {
        appendMessage(message);
    }
}

void DirectMessageView::appendMessage(const DirectMessageInfo& message) {
    new QListWidgetItem(tr("%1: %2").arg(message.author, message.body), messagesList_);
    messagesList_->scrollToBottom();
}

void DirectMessageView::onSendClicked() {
    const QString body = messageEdit_->text().trimmed();
    if (body.isEmpty()) {
        return;
    }
    emit sendMessageRequested(body);
    messageEdit_->clear();
}

}  // namespace devicehub
