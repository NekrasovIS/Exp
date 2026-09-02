#include "ui/LoginWindow.h"

#include <QHBoxLayout>
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
constexpr int kPasswordStepIndex = 2;
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
    auto* identifierHint = new QLabel(
        tr("Enter your login, email or Telegram chat ID — we'll send you a one-time sign-in code."),
        identifierPage);
    identifierHint->setWordWrap(true);
    identifierLayout->addWidget(identifierHint);

    identifierEdit_ = new QLineEdit(identifierPage);
    identifierEdit_->setObjectName(QStringLiteral("loginIdentifierEdit"));
    identifierEdit_->setPlaceholderText(tr("Login, email or Telegram chat ID"));
    connect(identifierEdit_, &QLineEdit::returnPressed, this, &LoginWindow::onRequestCodeClicked);
    identifierLayout->addWidget(identifierEdit_);

    requestCodeButton_ = new QPushButton(tr("Send Code"), identifierPage);
    requestCodeButton_->setObjectName(QStringLiteral("requestCodeButton"));
    requestCodeButton_->setProperty("accent", true);
    connect(requestCodeButton_, &QPushButton::clicked, this, &LoginWindow::onRequestCodeClicked);
    identifierLayout->addWidget(requestCodeButton_);

    usePasswordButton_ = new QPushButton(tr("Sign in with password instead"), identifierPage);
    usePasswordButton_->setObjectName(QStringLiteral("usePasswordButton"));
    usePasswordButton_->setFlat(true);
    connect(usePasswordButton_, &QPushButton::clicked, this, [this]() {
        statusLabel_->clear();
        stack_->setCurrentIndex(kPasswordStepIndex);
        passwordLoginEdit_->setFocus();
    });
    identifierLayout->addWidget(usePasswordButton_);

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

    auto* passwordPage = new QWidget(stack_);
    auto* passwordLayout = new QVBoxLayout(passwordPage);
    passwordLayout->setSpacing(ui_theme::kSpacingSm);

    auto* passwordHint = new QLabel(tr("Sign in with your login and password, or register a new account."),
                                     passwordPage);
    passwordHint->setWordWrap(true);
    passwordLayout->addWidget(passwordHint);

    passwordLoginEdit_ = new QLineEdit(passwordPage);
    passwordLoginEdit_->setObjectName(QStringLiteral("loginPasswordLoginEdit"));
    passwordLoginEdit_->setPlaceholderText(tr("Login"));
    passwordLayout->addWidget(passwordLoginEdit_);

    passwordEdit_ = new QLineEdit(passwordPage);
    passwordEdit_->setObjectName(QStringLiteral("loginPasswordEdit"));
    passwordEdit_->setPlaceholderText(tr("Password"));
    passwordEdit_->setEchoMode(QLineEdit::Password);
    connect(passwordEdit_, &QLineEdit::returnPressed, this, &LoginWindow::onPasswordSignInClicked);
    passwordLayout->addWidget(passwordEdit_);

    auto* passwordActionsRow = new QHBoxLayout;
    passwordSignInButton_ = new QPushButton(tr("Sign In"), passwordPage);
    passwordSignInButton_->setObjectName(QStringLiteral("passwordSignInButton"));
    passwordSignInButton_->setProperty("accent", true);
    connect(passwordSignInButton_, &QPushButton::clicked, this, &LoginWindow::onPasswordSignInClicked);
    registerButton_ = new QPushButton(tr("Register"), passwordPage);
    registerButton_->setObjectName(QStringLiteral("loginRegisterButton"));
    connect(registerButton_, &QPushButton::clicked, this, &LoginWindow::onRegisterClicked);
    passwordActionsRow->addWidget(passwordSignInButton_);
    passwordActionsRow->addWidget(registerButton_);
    passwordLayout->addLayout(passwordActionsRow);

    backToCodeButton_ = new QPushButton(tr("Use a one-time code instead"), passwordPage);
    backToCodeButton_->setObjectName(QStringLiteral("backToCodeButton"));
    connect(backToCodeButton_, &QPushButton::clicked, this, [this]() {
        statusLabel_->clear();
        stack_->setCurrentIndex(kIdentifierStepIndex);
    });
    passwordLayout->addWidget(backToCodeButton_);

    stack_->insertWidget(kIdentifierStepIndex, identifierPage);
    stack_->insertWidget(kCodeStepIndex, codePage);
    stack_->insertWidget(kPasswordStepIndex, passwordPage);
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

void LoginWindow::onPasswordSignInClicked() {
    const QString login = passwordLoginEdit_->text().trimmed();
    const QString password = passwordEdit_->text();
    if (login.isEmpty() || password.isEmpty()) {
        statusLabel_->setText(tr("Enter both login and password."));
        return;
    }
    statusLabel_->clear();
    emit passwordSignInRequested(login, password);
}

void LoginWindow::onRegisterClicked() {
    const QString login = passwordLoginEdit_->text().trimmed();
    const QString password = passwordEdit_->text();
    if (login.isEmpty() || password.isEmpty()) {
        statusLabel_->setText(tr("Enter both login and password."));
        return;
    }
    statusLabel_->clear();
    emit registerRequested(login, password);
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
    passwordLoginEdit_->clear();
    passwordEdit_->clear();
    statusLabel_->clear();
    pendingIdentifier_.clear();
}

}  // namespace devicehub
