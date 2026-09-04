#include "ui/CommunityConnectDialog.h"

#include <gtest/gtest.h>

#include <QLineEdit>
#include <QPushButton>
#include <QSignalSpy>

namespace devicehub {
namespace {

TEST(CommunityConnectDialogTest, ClickingJoinWithACodeEmitsJoinRequestedAndClearsTheField) {
    CommunityConnectDialog dialog;
    dialog.inviteCodeEdit()->setText(QStringLiteral("ABC123"));
    QSignalSpy spy(&dialog, &CommunityConnectDialog::joinRequested);

    dialog.joinButton()->click();

    ASSERT_EQ(spy.count(), 1);
    EXPECT_EQ(spy.at(0).at(0).toString(), QStringLiteral("ABC123"));
    EXPECT_TRUE(dialog.inviteCodeEdit()->text().isEmpty());
}

TEST(CommunityConnectDialogTest, ClickingJoinWithAnEmptyCodeEmitsNothing) {
    CommunityConnectDialog dialog;
    QSignalSpy spy(&dialog, &CommunityConnectDialog::joinRequested);

    dialog.joinButton()->click();

    EXPECT_EQ(spy.count(), 0);
}

TEST(CommunityConnectDialogTest, PressingEnterInInviteCodeEditEmitsJoinRequested) {
    CommunityConnectDialog dialog;
    dialog.inviteCodeEdit()->setText(QStringLiteral("ABC123"));
    QSignalSpy spy(&dialog, &CommunityConnectDialog::joinRequested);

    emit dialog.inviteCodeEdit()->returnPressed();

    EXPECT_EQ(spy.count(), 1);
}

TEST(CommunityConnectDialogTest, ClickingCreateWithANameEmitsCreateRequestedAndClearsTheField) {
    CommunityConnectDialog dialog;
    dialog.nameEdit()->setText(QStringLiteral("My Community"));
    QSignalSpy spy(&dialog, &CommunityConnectDialog::createRequested);

    dialog.createButton()->click();

    ASSERT_EQ(spy.count(), 1);
    EXPECT_EQ(spy.at(0).at(0).toString(), QStringLiteral("My Community"));
    EXPECT_TRUE(dialog.nameEdit()->text().isEmpty());
}

TEST(CommunityConnectDialogTest, ClickingCreateWithAnEmptyNameEmitsNothing) {
    CommunityConnectDialog dialog;
    QSignalSpy spy(&dialog, &CommunityConnectDialog::createRequested);

    dialog.createButton()->click();

    EXPECT_EQ(spy.count(), 0);
}

TEST(CommunityConnectDialogTest, PressingEnterInNameEditEmitsCreateRequested) {
    CommunityConnectDialog dialog;
    dialog.nameEdit()->setText(QStringLiteral("My Community"));
    QSignalSpy spy(&dialog, &CommunityConnectDialog::createRequested);

    emit dialog.nameEdit()->returnPressed();

    EXPECT_EQ(spy.count(), 1);
}

}  // namespace
}  // namespace devicehub
