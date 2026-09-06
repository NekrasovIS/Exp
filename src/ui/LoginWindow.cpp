#include "ui/LoginWindow.h"

#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>

#include "ui/Theme.h"

namespace devicehub {

namespace {
constexpr int kIdentifierStepIndex = 0;
constexpr int kCodeStepIndex = 1;
}  // namespace

LoginWindow::LoginWindow(QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Sign In"));
    setModal(true);

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setSpacing(ui_theme::kSpacingMd);

    stack_ = new QStackedWidget(this);

    auto* identifierPage = new QWidget(stack_);
    auto* identifierLayout = new QVBoxLayout(identifierPage);
    identifierLayout->setSpacing(ui_theme::kSpacingSm);
    auto* identifierHint =
        new QLabel(tr("Enter your login or email — we'll send you a one-time sign-in code."), identifierPage);
    identifierHint->setWordWrap(true);
    identifierLayout->addWidget(identifierHint);

    identifierEdit_ = new QLineEdit(identifierPage);
    identifierEdit_->setObjectName(QStringLiteral("loginIdentifierEdit"));
    identifierEdit_->setPlaceholderText(tr("Login or email"));
    connect(identifierEdit_, &QLineEdit::returnPressed, this, &LoginWindow::onRequestCodeClicked);
    identifierLayout->addWidget(identifierEdit_);

    requestCodeButton_ = new QPushButton(tr("Send Code"), identifierPage);
    requestCodeButton_->setObjectName(QStringLiteral("requestCodeButton"));
    requestCodeButton_->setProperty("accent", true);
    connect(requestCodeButton_, &QPushButton::clicked, this, &LoginWindow::onRequestCodeClicked);
    identifierLayout->addWidget(requestCodeButton_);

    auto* codePage = new QWidget(stack_);
    auto* codeLayout = new QVBoxLayout(codePage);
    codeLayout->setSpacing(ui_theme::kSpacingSm);

    codeSentLabel_ = new QLabel(codePage);
    codeSentLabel_->setObjectName(QStringLiteral("loginCodeSentLabel"));
    codeSentLabel_->setWordWrap(true);
    codeLayout->addWidget(codeSentLabel_);

    codeEdit_ = new QLineEdit(codePage);
    codeEdit_->setObjectName(QStringLiteral("loginCodeEdit"));
    codeEdit_->setPlaceholderText(tr("6-digit code"));
    connect(codeEdit_, &QLineEdit::returnPressed, this, &LoginWindow::onVerifyCodeClicked);
    codeLayout->addWidget(codeEdit_);

    verifyCodeButton_ = new QPushButton(tr("Sign In"), codePage);
    verifyCodeButton_->setObjectName(QStringLiteral("verifyCodeButton"));
    verifyCodeButton_->setProperty("accent", true);
    connect(verifyCodeButton_, &QPushButton::clicked, this, &LoginWindow::onVerifyCodeClicked);
    codeLayout->addWidget(verifyCodeButton_);

    backButton_ = new QPushButton(tr("Use a different login or email"), codePage);
    backButton_->setObjectName(QStringLiteral("loginBackButton"));
    connect(backButton_, &QPushButton::clicked, this, &LoginWindow::onBackClicked);
    codeLayout->addWidget(backButton_);

    stack_->insertWidget(kIdentifierStepIndex, identifierPage);
    stack_->insertWidget(kCodeStepIndex, codePage);
    stack_->setCurrentIndex(kIdentifierStepIndex);
    rootLayout->addWidget(stack_);

    statusLabel_ = new QLabel(this);
    statusLabel_->setObjectName(QStringLiteral("loginStatusLabel"));
    statusLabel_->setWordWrap(true);
    rootLayout->addWidget(statusLabel_);
}

void LoginWindow::onRequestCodeClicked() {
    const QString identifier = identifierEdit_->text().trimmed();
    if (identifier.isEmpty()) {
        statusLabel_->setText(tr("Enter your login or email first."));
        return;
    }
    pendingIdentifier_ = identifier;
    statusLabel_->clear();
    emit requestCodeRequested(identifier);
}

void LoginWindow::onVerifyCodeClicked() {
    const QString code = codeEdit_->text().trimmed();
    if (code.isEmpty()) {
        statusLabel_->setText(tr("Enter the code we sent you."));
        return;
    }
    statusLabel_->clear();
    emit verifyCodeRequested(pendingIdentifier_, code);
}

void LoginWindow::onBackClicked() {
    stack_->setCurrentIndex(kIdentifierStepIndex);
    codeEdit_->clear();
    statusLabel_->clear();
}

void LoginWindow::showCodeSent(const QString& identifier) {
    pendingIdentifier_ = identifier;
    codeSentLabel_->setText(
        tr("We sent a code to %1 — enter it below. It expires in a few minutes.").arg(identifier));
    codeEdit_->clear();
    statusLabel_->clear();
    stack_->setCurrentIndex(kCodeStepIndex);
    codeEdit_->setFocus();
}

void LoginWindow::showError(const QString& message) {
    statusLabel_->setText(tr("Error: %1").arg(message));
}

void LoginWindow::reset() {
    stack_->setCurrentIndex(kIdentifierStepIndex);
    identifierEdit_->clear();
    codeEdit_->clear();
    statusLabel_->clear();
    pendingIdentifier_.clear();
}

}  // namespace devicehub
