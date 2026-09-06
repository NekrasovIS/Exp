#include "ui/AccountMenu.h"

#include <gtest/gtest.h>

#include <QFrame>
#include <QPushButton>

namespace devicehub {
namespace {

TEST(AccountMenuTest, PopupStartsHidden) {
    AccountMenu menu;

    const QFrame* popup = menu.findChild<QFrame*>(QStringLiteral("accountMenuPopup"));
    ASSERT_NE(popup, nullptr);
    EXPECT_TRUE(popup->isHidden());
}

TEST(AccountMenuTest, ClickingToggleButtonShowsThenHidesThePopup) {
    AccountMenu menu;
    QFrame* popup = menu.findChild<QFrame*>(QStringLiteral("accountMenuPopup"));
    ASSERT_NE(popup, nullptr);

    emit menu.toggleButton()->clicked();
    EXPECT_FALSE(popup->isHidden());

    emit menu.toggleButton()->clicked();
    EXPECT_TRUE(popup->isHidden());
}

TEST(AccountMenuTest, EditProfileButtonStartsDisabledAndFollowsSetEditProfileEnabled) {
    AccountMenu menu;

    QPushButton* editProfileButton = menu.findChild<QPushButton*>(QStringLiteral("editProfileButton"));
    ASSERT_NE(editProfileButton, nullptr);
    EXPECT_FALSE(editProfileButton->isEnabled());

    menu.setEditProfileEnabled(true);
    EXPECT_TRUE(editProfileButton->isEnabled());

    menu.setEditProfileEnabled(false);
    EXPECT_FALSE(editProfileButton->isEnabled());
}

}  // namespace
}  // namespace devicehub
