#include "ui/TotpSetupDialog.h"

#include <gtest/gtest.h>

#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalSpy>

#include "user/UserProfileClient.h"

namespace devicehub {
namespace {

TEST(TotpSetupDialogTest, ShowStatusDisabledShowsEnableAndHidesDisableForm) {
    TotpSetupDialog dialog;

    dialog.showStatus(false);

    EXPECT_FALSE(dialog.enableButton()->isHidden());
    EXPECT_TRUE(dialog.disableCodeEdit()->isHidden());
    EXPECT_TRUE(dialog.disableButton()->isHidden());
    EXPECT_TRUE(dialog.statusTextLabel()->text().contains(QStringLiteral("disabled")));
}

TEST(TotpSetupDialogTest, ShowStatusEnabledShowsDisableFormAndHidesEnable) {
    TotpSetupDialog dialog;

    dialog.showStatus(true);

    EXPECT_TRUE(dialog.enableButton()->isHidden());
    EXPECT_FALSE(dialog.disableCodeEdit()->isHidden());
    EXPECT_FALSE(dialog.disableButton()->isHidden());
    EXPECT_TRUE(dialog.statusTextLabel()->text().contains(QStringLiteral("enabled")));
}

TEST(TotpSetupDialogTest, ClickingEnableEmitsEnableRequested) {
    TotpSetupDialog dialog;
    dialog.showStatus(false);
    QSignalSpy spy(&dialog, &TotpSetupDialog::enableRequested);

    dialog.enableButton()->click();

    EXPECT_EQ(spy.count(), 1);
}

TEST(TotpSetupDialogTest, ShowSetupSwitchesStepAndDisplaysSecret) {
    TotpSetupDialog dialog;
    dialog.showStatus(false);

    EXPECT_TRUE(dialog.confirmCodeEdit()->parentWidget()->isHidden());

    dialog.showSetup(TotpSetupInfo{.secret = QStringLiteral("JBSWY3DPEHPK3PXP"),
                                    .otpauthUrl = QStringLiteral("otpauth://totp/DeviceHub:alice?secret=JBSWY3DPEHPK3PXP")});

    EXPECT_FALSE(dialog.confirmCodeEdit()->parentWidget()->isHidden());
    EXPECT_TRUE(dialog.secretLabel()->text().contains(QStringLiteral("JBSWY3DPEHPK3PXP")));
    EXPECT_FALSE(dialog.qrLabel()->pixmap().isNull());
}

TEST(TotpSetupDialogTest, ClickingConfirmWithEmptyCodeShowsStatusAndEmitsNothing) {
    TotpSetupDialog dialog;
    dialog.showSetup(TotpSetupInfo{.secret = QStringLiteral("SECRET"), .otpauthUrl = QStringLiteral("otpauth://x")});
    QSignalSpy spy(&dialog, &TotpSetupDialog::confirmRequested);

    dialog.confirmButton()->click();

    EXPECT_EQ(spy.count(), 0);
    EXPECT_FALSE(dialog.statusLabel()->text().isEmpty());
}

TEST(TotpSetupDialogTest, ClickingConfirmEmitsConfirmRequestedWithTheCode) {
    TotpSetupDialog dialog;
    dialog.showSetup(TotpSetupInfo{.secret = QStringLiteral("SECRET"), .otpauthUrl = QStringLiteral("otpauth://x")});
    dialog.confirmCodeEdit()->setText(QStringLiteral("123456"));
    QSignalSpy spy(&dialog, &TotpSetupDialog::confirmRequested);

    dialog.confirmButton()->click();

    ASSERT_EQ(spy.count(), 1);
    EXPECT_EQ(spy.at(0).at(0).toString(), QStringLiteral("123456"));
}

TEST(TotpSetupDialogTest, ClickingCancelSetupReturnsToDisabledStatusWithoutEmittingAnything) {
    TotpSetupDialog dialog;
    dialog.showSetup(TotpSetupInfo{.secret = QStringLiteral("SECRET"), .otpauthUrl = QStringLiteral("otpauth://x")});
    QSignalSpy confirmSpy(&dialog, &TotpSetupDialog::confirmRequested);
    QSignalSpy enableSpy(&dialog, &TotpSetupDialog::enableRequested);

    dialog.cancelSetupButton()->click();

    EXPECT_TRUE(dialog.confirmCodeEdit()->parentWidget()->isHidden());
    EXPECT_FALSE(dialog.enableButton()->isHidden());
    EXPECT_EQ(confirmSpy.count(), 0);
    EXPECT_EQ(enableSpy.count(), 0);
}

TEST(TotpSetupDialogTest, ShowBackupCodesSwitchesStepAndListsEveryCode) {
    TotpSetupDialog dialog;
    dialog.showSetup(TotpSetupInfo{.secret = QStringLiteral("SECRET"), .otpauthUrl = QStringLiteral("otpauth://x")});

    dialog.showBackupCodes({QStringLiteral("AAAA-1111"), QStringLiteral("BBBB-2222")});

    EXPECT_FALSE(dialog.backupCodesLabel()->parentWidget()->isHidden());
    EXPECT_TRUE(dialog.backupCodesLabel()->text().contains(QStringLiteral("AAAA-1111")));
    EXPECT_TRUE(dialog.backupCodesLabel()->text().contains(QStringLiteral("BBBB-2222")));
}

TEST(TotpSetupDialogTest, ClickingBackupCodesAcknowledgedReturnsToEnabledStatus) {
    TotpSetupDialog dialog;
    dialog.showBackupCodes({QStringLiteral("AAAA-1111")});

    dialog.backupCodesAcknowledgedButton()->click();

    EXPECT_TRUE(dialog.enableButton()->isHidden());
    EXPECT_FALSE(dialog.disableButton()->isHidden());
    EXPECT_TRUE(dialog.statusTextLabel()->text().contains(QStringLiteral("enabled")));
}

TEST(TotpSetupDialogTest, ClickingDisableWithEmptyCodeShowsStatusAndEmitsNothing) {
    TotpSetupDialog dialog;
    dialog.showStatus(true);
    QSignalSpy spy(&dialog, &TotpSetupDialog::disableRequested);

    dialog.disableButton()->click();

    EXPECT_EQ(spy.count(), 0);
    EXPECT_FALSE(dialog.statusLabel()->text().isEmpty());
}

TEST(TotpSetupDialogTest, ClickingDisableEmitsDisableRequestedWithTheCode) {
    TotpSetupDialog dialog;
    dialog.showStatus(true);
    dialog.disableCodeEdit()->setText(QStringLiteral("654321"));
    QSignalSpy spy(&dialog, &TotpSetupDialog::disableRequested);

    dialog.disableButton()->click();

    ASSERT_EQ(spy.count(), 1);
    EXPECT_EQ(spy.at(0).at(0).toString(), QStringLiteral("654321"));
}

TEST(TotpSetupDialogTest, ShowErrorSetsStatusLabelTextWithoutChangingStep) {
    TotpSetupDialog dialog;
    dialog.showStatus(true);

    dialog.showError(QStringLiteral("invalid code"));

    EXPECT_TRUE(dialog.statusLabel()->text().contains(QStringLiteral("invalid code")));
    EXPECT_FALSE(dialog.disableButton()->isHidden());
}

}  // namespace
}  // namespace devicehub
