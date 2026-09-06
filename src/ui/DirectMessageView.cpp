#include "ui/DirectMessageView.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

#include "ui/Theme.h"

namespace devicehub {

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

    auto* sendRow = new QHBoxLayout;
    sendRow->setSpacing(ui_theme::kSpacingSm);
    messageEdit_ = new QLineEdit(this);
    messageEdit_->setObjectName(QStringLiteral("dmMessageEdit"));
    messageEdit_->setPlaceholderText(tr("Message"));
    messageEdit_->setEnabled(false);
    connect(messageEdit_, &QLineEdit::returnPressed, this, &DirectMessageView::onSendClicked);

    sendButton_ = new QPushButton(tr("Send"), this);
    sendButton_->setObjectName(QStringLiteral("sendDmButton"));
    sendButton_->setProperty("accent", true);
    sendButton_->setEnabled(false);
    connect(sendButton_, &QPushButton::clicked, this, &DirectMessageView::onSendClicked);

    sendRow->addWidget(messageEdit_, /*stretch=*/1);
    sendRow->addWidget(sendButton_);

    layout->addWidget(titleLabel_);
    layout->addWidget(messagesList_, /*stretch=*/1);
    layout->addLayout(sendRow);
}

void DirectMessageView::showPlaceholder() {
    titleLabel_->setText(tr("Select a friend to start a conversation"));
    messagesList_->clear();
    messageEdit_->clear();
    messageEdit_->setEnabled(false);
    sendButton_->setEnabled(false);
}

void DirectMessageView::showThread(const QString& otherLogin) {
    titleLabel_->setText(otherLogin);
    messagesList_->clear();
    messageEdit_->setEnabled(true);
    sendButton_->setEnabled(true);
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
