#include "ui/ModeratorsDialog.h"

#include <gtest/gtest.h>

#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>

namespace devicehub {
namespace {

TEST(ModeratorsDialogTest, StartsWithAnEmptyListAndNoCommunitySelected) {
    ModeratorsDialog dialog;

    EXPECT_EQ(dialog.moderatorsList()->count(), 0);
    EXPECT_EQ(dialog.communityId(), -1);
}

TEST(ModeratorsDialogTest, SetCommunityStoresTheIdForLaterSignals) {
    ModeratorsDialog dialog;

    dialog.setCommunity(42, QStringLiteral("Test Community"));

    EXPECT_EQ(dialog.communityId(), 42);
}

TEST(ModeratorsDialogTest, SetModeratorsPopulatesTheList) {
    ModeratorsDialog dialog;

    dialog.setModerators({QStringLiteral("alice"), QStringLiteral("bob")});

    ASSERT_EQ(dialog.moderatorsList()->count(), 2);
    EXPECT_EQ(dialog.moderatorsList()->item(0)->text(), QStringLiteral("alice"));
    EXPECT_EQ(dialog.moderatorsList()->item(1)->text(), QStringLiteral("bob"));
}

TEST(ModeratorsDialogTest, SetModeratorsReplacesPreviousContents) {
    ModeratorsDialog dialog;
    dialog.setModerators({QStringLiteral("alice")});

    dialog.setModerators({QStringLiteral("carol")});

    ASSERT_EQ(dialog.moderatorsList()->count(), 1);
    EXPECT_EQ(dialog.moderatorsList()->item(0)->text(), QStringLiteral("carol"));
}

TEST(ModeratorsDialogTest, ClickingPromoteEmitsPromoteRequestedWithTheTypedLoginAndClearsTheField) {
    ModeratorsDialog dialog;
    dialog.setCommunity(7, QStringLiteral("Test Community"));
    dialog.loginEdit()->setText(QStringLiteral("dave"));

    qint64 emittedCommunityId = -1;
    QString emittedLogin;
    int emitCount = 0;
    QObject::connect(&dialog, &ModeratorsDialog::promoteRequested, [&](qint64 communityId, const QString& login) {
        emittedCommunityId = communityId;
        emittedLogin = login;
        ++emitCount;
    });

    emit dialog.promoteButton()->clicked();

    EXPECT_EQ(emitCount, 1);
    EXPECT_EQ(emittedCommunityId, 7);
    EXPECT_EQ(emittedLogin, QStringLiteral("dave"));
    EXPECT_TRUE(dialog.loginEdit()->text().isEmpty());
}

TEST(ModeratorsDialogTest, ClickingPromoteWithAnEmptyFieldEmitsNothing) {
    ModeratorsDialog dialog;
    dialog.setCommunity(7, QStringLiteral("Test Community"));

    int emitCount = 0;
    QObject::connect(&dialog, &ModeratorsDialog::promoteRequested, [&](qint64, const QString&) { ++emitCount; });

    emit dialog.promoteButton()->clicked();

    EXPECT_EQ(emitCount, 0);
}

TEST(ModeratorsDialogTest, ClickingDemoteSelectedEmitsDemoteRequestedForTheSelectedItem) {
    ModeratorsDialog dialog;
    dialog.setCommunity(7, QStringLiteral("Test Community"));
    dialog.setModerators({QStringLiteral("alice"), QStringLiteral("bob")});
    dialog.moderatorsList()->setCurrentRow(1);

    qint64 emittedCommunityId = -1;
    QString emittedLogin;
    int emitCount = 0;
    QObject::connect(&dialog, &ModeratorsDialog::demoteRequested, [&](qint64 communityId, const QString& login) {
        emittedCommunityId = communityId;
        emittedLogin = login;
        ++emitCount;
    });

    emit dialog.demoteButton()->clicked();

    EXPECT_EQ(emitCount, 1);
    EXPECT_EQ(emittedCommunityId, 7);
    EXPECT_EQ(emittedLogin, QStringLiteral("bob"));
}

TEST(ModeratorsDialogTest, ClickingDemoteSelectedWithNoSelectionEmitsNothing) {
    ModeratorsDialog dialog;
    dialog.setCommunity(7, QStringLiteral("Test Community"));
    dialog.setModerators({QStringLiteral("alice")});

    int emitCount = 0;
    QObject::connect(&dialog, &ModeratorsDialog::demoteRequested, [&](qint64, const QString&) { ++emitCount; });

    emit dialog.demoteButton()->clicked();

    EXPECT_EQ(emitCount, 0);
}

}  // namespace
}  // namespace devicehub
