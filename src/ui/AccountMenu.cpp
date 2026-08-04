#include "ui/AccountMenu.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace devicehub {

AccountMenu::AccountMenu(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_StyledBackground, true);

    auto* outerLayout = new QHBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);

    toggleButton_ = new QPushButton(tr("Account"), this);
    toggleButton_->setObjectName(QStringLiteral("accountMenuButton"));
    outerLayout->addWidget(toggleButton_);

    popup_ = new QFrame(this, Qt::Popup);
    popup_->setObjectName(QStringLiteral("accountMenuPopup"));
    popup_->setFrameShape(QFrame::StyledPanel);
    auto* popupLayout = new QVBoxLayout(popup_);

    loginEdit_ = new QLineEdit(popup_);
    loginEdit_->setObjectName(QStringLiteral("loginEdit"));
    loginEdit_->setPlaceholderText(tr("Login"));

    passwordEdit_ = new QLineEdit(popup_);
    passwordEdit_->setObjectName(QStringLiteral("passwordEdit"));
    passwordEdit_->setPlaceholderText(tr("Password"));
    passwordEdit_->setEchoMode(QLineEdit::Password);

    requestTokenButton_ = new QPushButton(tr("Get token & verify"), popup_);
    requestTokenButton_->setObjectName(QStringLiteral("requestTokenButton"));
    requestTokenButton_->setProperty("accent", true);

    registerButton_ = new QPushButton(tr("Register"), popup_);
    registerButton_->setObjectName(QStringLiteral("registerButton"));

    auto* actionsLayout = new QHBoxLayout;
    actionsLayout->addWidget(requestTokenButton_);
    actionsLayout->addWidget(registerButton_);

    statusLabel_ = new QLabel(tr("No token requested yet"), popup_);
    statusLabel_->setObjectName(QStringLiteral("authStatusLabel"));
    statusLabel_->setWordWrap(true);

    popupLayout->addWidget(loginEdit_);
    popupLayout->addWidget(passwordEdit_);
    popupLayout->addLayout(actionsLayout);
    popupLayout->addWidget(statusLabel_);

    connect(toggleButton_, &QPushButton::clicked, this, &AccountMenu::togglePopup);
}

void AccountMenu::togglePopup() {
    if (popup_->isVisible()) {
        popup_->hide();
        return;
    }

    const QPoint anchor = toggleButton_->mapToGlobal(QPoint(0, toggleButton_->height()));
    popup_->move(anchor.x() + toggleButton_->width() - popup_->sizeHint().width(), anchor.y());
    popup_->show();
}

}  // namespace devicehub
