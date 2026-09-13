#include "ui/TotpSetupDialog.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>

#include "ui/QrCodeGenerator.h"
#include "ui/Theme.h"
#include "user/UserProfileClient.h"

namespace devicehub {

namespace {
constexpr int kStatusStepIndex = 0;
constexpr int kSetupStepIndex = 1;
constexpr int kBackupCodesStepIndex = 2;
}  // namespace

TotpSetupDialog::TotpSetupDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Two-Factor Authentication"));
    setObjectName(QStringLiteral("totpSetupDialog"));

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setSpacing(ui_theme::kSpacingMd);

    stack_ = new QStackedWidget(this);

    auto* statusPage = new QWidget(stack_);
    auto* statusLayout = new QVBoxLayout(statusPage);
    statusLayout->setSpacing(ui_theme::kSpacingSm);

    statusTextLabel_ = new QLabel(statusPage);
    statusTextLabel_->setObjectName(QStringLiteral("totpStatusTextLabel"));
    statusTextLabel_->setWordWrap(true);
    statusLayout->addWidget(statusTextLabel_);

    enableButton_ = new QPushButton(tr("Enable"), statusPage);
    enableButton_->setObjectName(QStringLiteral("totpEnableButton"));
    enableButton_->setProperty("accent", true);
    connect(enableButton_, &QPushButton::clicked, this, [this]() { emit enableRequested(); });
    statusLayout->addWidget(enableButton_);

    disableCodeEdit_ = new QLineEdit(statusPage);
    disableCodeEdit_->setObjectName(QStringLiteral("totpDisableCodeEdit"));
    disableCodeEdit_->setPlaceholderText(tr("Current code, to disable"));
    connect(disableCodeEdit_, &QLineEdit::returnPressed, this, &TotpSetupDialog::onDisableClicked);
    statusLayout->addWidget(disableCodeEdit_);

    disableButton_ = new QPushButton(tr("Disable"), statusPage);
    disableButton_->setObjectName(QStringLiteral("totpDisableButton"));
    connect(disableButton_, &QPushButton::clicked, this, &TotpSetupDialog::onDisableClicked);
    statusLayout->addWidget(disableButton_);

    auto* setupPage = new QWidget(stack_);
    auto* setupLayout = new QVBoxLayout(setupPage);
    setupLayout->setSpacing(ui_theme::kSpacingSm);

    qrLabel_ = new QLabel(setupPage);
    qrLabel_->setObjectName(QStringLiteral("totpQrLabel"));
    qrLabel_->setAlignment(Qt::AlignCenter);
    setupLayout->addWidget(qrLabel_);

    secretLabel_ = new QLabel(setupPage);
    secretLabel_->setObjectName(QStringLiteral("totpSecretLabel"));
    secretLabel_->setWordWrap(true);
    secretLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    setupLayout->addWidget(secretLabel_);

    confirmCodeEdit_ = new QLineEdit(setupPage);
    confirmCodeEdit_->setObjectName(QStringLiteral("totpConfirmCodeEdit"));
    confirmCodeEdit_->setPlaceholderText(tr("Confirmation code"));
    connect(confirmCodeEdit_, &QLineEdit::returnPressed, this, &TotpSetupDialog::onConfirmClicked);
    setupLayout->addWidget(confirmCodeEdit_);

    auto* setupActionsRow = new QHBoxLayout;
    confirmButton_ = new QPushButton(tr("Confirm"), setupPage);
    confirmButton_->setObjectName(QStringLiteral("totpConfirmButton"));
    confirmButton_->setProperty("accent", true);
    connect(confirmButton_, &QPushButton::clicked, this, &TotpSetupDialog::onConfirmClicked);
    cancelSetupButton_ = new QPushButton(tr("Cancel"), setupPage);
    cancelSetupButton_->setObjectName(QStringLiteral("totpCancelSetupButton"));
    connect(cancelSetupButton_, &QPushButton::clicked, this, &TotpSetupDialog::onCancelSetupClicked);
    setupActionsRow->addWidget(confirmButton_);
    setupActionsRow->addWidget(cancelSetupButton_);
    setupLayout->addLayout(setupActionsRow);

    auto* backupCodesPage = new QWidget(stack_);
    auto* backupCodesLayout = new QVBoxLayout(backupCodesPage);
    backupCodesLayout->setSpacing(ui_theme::kSpacingSm);

    auto* backupCodesWarning = new QLabel(
        tr("Save these backup codes somewhere safe — each works once if you lose access to your "
           "authenticator app, and they won't be shown again."),
        backupCodesPage);
    backupCodesWarning->setWordWrap(true);
    backupCodesLayout->addWidget(backupCodesWarning);

    backupCodesLabel_ = new QLabel(backupCodesPage);
    backupCodesLabel_->setObjectName(QStringLiteral("totpBackupCodesLabel"));
    backupCodesLabel_->setWordWrap(true);
    backupCodesLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    backupCodesLayout->addWidget(backupCodesLabel_);

    backupCodesAcknowledgedButton_ = new QPushButton(tr("I've saved these codes"), backupCodesPage);
    backupCodesAcknowledgedButton_->setObjectName(QStringLiteral("totpBackupCodesAcknowledgedButton"));
    backupCodesAcknowledgedButton_->setProperty("accent", true);
    connect(backupCodesAcknowledgedButton_, &QPushButton::clicked, this,
            &TotpSetupDialog::onBackupCodesAcknowledgedClicked);
    backupCodesLayout->addWidget(backupCodesAcknowledgedButton_);

    stack_->insertWidget(kStatusStepIndex, statusPage);
    stack_->insertWidget(kSetupStepIndex, setupPage);
    stack_->insertWidget(kBackupCodesStepIndex, backupCodesPage);
    stack_->setCurrentIndex(kStatusStepIndex);
    rootLayout->addWidget(stack_);

    statusLabel_ = new QLabel(this);
    statusLabel_->setObjectName(QStringLiteral("totpDialogStatusLabel"));
    statusLabel_->setWordWrap(true);
    rootLayout->addWidget(statusLabel_);
}

void TotpSetupDialog::onConfirmClicked() {
    const QString code = confirmCodeEdit_->text().trimmed();
    if (code.isEmpty()) {
        statusLabel_->setText(tr("Enter the confirmation code."));
        return;
    }
    statusLabel_->clear();
    emit confirmRequested(code);
}

void TotpSetupDialog::onCancelSetupClicked() {
    // Ничего не сохранялось на сервере до confirmTotp(), поэтому отмена
    // — чисто локальный откат, TOTP заведомо ещё выключена.
    showStatus(false);
}

void TotpSetupDialog::onDisableClicked() {
    const QString code = disableCodeEdit_->text().trimmed();
    if (code.isEmpty()) {
        statusLabel_->setText(tr("Enter your current authentication code."));
        return;
    }
    statusLabel_->clear();
    emit disableRequested(code);
}

void TotpSetupDialog::onBackupCodesAcknowledgedClicked() {
    showStatus(true);
}

void TotpSetupDialog::showStatus(bool enabled) {
    statusTextLabel_->setText(enabled ? tr("Two-factor authentication is enabled for your account.")
                                       : tr("Two-factor authentication is currently disabled."));
    enableButton_->setVisible(!enabled);
    disableCodeEdit_->setVisible(enabled);
    disableButton_->setVisible(enabled);
    disableCodeEdit_->clear();
    statusLabel_->clear();
    stack_->setCurrentIndex(kStatusStepIndex);
}

void TotpSetupDialog::showSetup(const TotpSetupInfo& setup) {
    const QImage qrImage = qr_code_generator::encode(setup.otpauthUrl);
    qrLabel_->setPixmap(qrImage.isNull() ? QPixmap() : QPixmap::fromImage(qrImage));
    secretLabel_->setText(tr("Scan this with your authenticator app, or enter the secret manually: %1")
                               .arg(setup.secret));
    confirmCodeEdit_->clear();
    statusLabel_->clear();
    stack_->setCurrentIndex(kSetupStepIndex);
    confirmCodeEdit_->setFocus();
}

void TotpSetupDialog::showBackupCodes(const QStringList& backupCodes) {
    backupCodesLabel_->setText(backupCodes.join(QStringLiteral("\n")));
    statusLabel_->clear();
    stack_->setCurrentIndex(kBackupCodesStepIndex);
}

void TotpSetupDialog::showError(const QString& message) {
    statusLabel_->setText(tr("Error: %1").arg(message));
}

}  // namespace devicehub
