#include "ui/MemberListPanel.h"

#include <gtest/gtest.h>

#include <QLabel>
#include <QListWidget>
#include <QSignalSpy>

namespace devicehub {
namespace {

TEST(MemberListPanelTest, StartsEmptyWithZeroCountTitle) {
    MemberListPanel panel;

    EXPECT_EQ(panel.listWidget()->count(), 0);
    EXPECT_EQ(panel.titleLabel()->text(), QStringLiteral("MEMBERS — 0"));
}

TEST(MemberListPanelTest, SetMembersPopulatesTheListAndTitleCount) {
    MemberListPanel panel;

    panel.setMembers({QStringLiteral("bob"), QStringLiteral("alice")});

    ASSERT_EQ(panel.listWidget()->count(), 2);
    EXPECT_EQ(panel.titleLabel()->text(), QStringLiteral("MEMBERS — 2"));
}

TEST(MemberListPanelTest, SetMembersSortsAlphabeticallyCaseInsensitive) {
    MemberListPanel panel;

    panel.setMembers({QStringLiteral("carol"), QStringLiteral("Alice"), QStringLiteral("bob")});

    ASSERT_EQ(panel.listWidget()->count(), 3);
    EXPECT_EQ(panel.listWidget()->item(0)->text(), QStringLiteral("Alice"));
    EXPECT_EQ(panel.listWidget()->item(1)->text(), QStringLiteral("bob"));
    EXPECT_EQ(panel.listWidget()->item(2)->text(), QStringLiteral("carol"));
}

TEST(MemberListPanelTest, SetMembersReplacesPreviousContents) {
    MemberListPanel panel;
    panel.setMembers({QStringLiteral("alice")});

    panel.setMembers({QStringLiteral("bob"), QStringLiteral("carol")});

    ASSERT_EQ(panel.listWidget()->count(), 2);
    EXPECT_EQ(panel.listWidget()->item(0)->text(), QStringLiteral("bob"));
    EXPECT_EQ(panel.listWidget()->item(1)->text(), QStringLiteral("carol"));
}

// issue #217: showContextMenu() (private, вызывается через
// customContextMenuRequested()) открывает реальный QMenu::exec()
// (блокирующий модальный вызов) только когда клик пришёлся на
// чужую строку в зашифрованном канале — эти тесты проверяют только
// более ранние guard-условия, ничего не кликая внутри самого меню, по
// тому же ограничению, что и у CommunitiesPanelTest (там его
// контекстное меню тоже не тестируется дальше самого открытия).

TEST(MemberListPanelTest, ContextMenuEmitsNothingWhenClickIsOutsideAnyItem) {
    MemberListPanel panel;
    panel.setChannelEncrypted(true);
    panel.setMembers({QStringLiteral("alice")});
    QSignalSpy spy(&panel, &MemberListPanel::grantChannelKeyAccessRequested);

    emit panel.listWidget()->customContextMenuRequested(QPoint(0, 10000));

    EXPECT_EQ(spy.count(), 0);
}

TEST(MemberListPanelTest, ContextMenuEmitsNothingWhenChannelIsNotEncrypted) {
    MemberListPanel panel;
    panel.setChannelEncrypted(false);
    panel.setMembers({QStringLiteral("alice")});
    QSignalSpy spy(&panel, &MemberListPanel::grantChannelKeyAccessRequested);

    const QPoint pos = panel.listWidget()->visualItemRect(panel.listWidget()->item(0)).center();
    emit panel.listWidget()->customContextMenuRequested(pos);

    EXPECT_EQ(spy.count(), 0);
}

// Issue #309 — setOnlineLogins()/setLoginOnline() only change which icon
// is drawn (memberAvatarIcon(..., online)), never the item's text() —
// showContextMenu() above and the sorting tests below both depend on
// text() staying exactly the login.

TEST(MemberListPanelTest, SetOnlineLoginsSurvivesAFollowingSetMembersCall) {
    MemberListPanel panel;
    panel.setMembers({QStringLiteral("alice"), QStringLiteral("bob")});
    panel.setOnlineLogins({QStringLiteral("alice")});

    // A REST refresh of the member list (setMembers()) shouldn't blank
    // out presence state that arrived separately over the WebSocket.
    panel.setMembers({QStringLiteral("alice"), QStringLiteral("bob"), QStringLiteral("carol")});

    ASSERT_EQ(panel.listWidget()->count(), 3);
    EXPECT_EQ(panel.listWidget()->item(0)->text(), QStringLiteral("alice"));
}

TEST(MemberListPanelTest, SetLoginOnlineTogglesWithoutChangingItemText) {
    MemberListPanel panel;
    panel.setMembers({QStringLiteral("alice"), QStringLiteral("bob")});

    panel.setLoginOnline(QStringLiteral("bob"), true);
    ASSERT_EQ(panel.listWidget()->count(), 2);
    EXPECT_EQ(panel.listWidget()->item(1)->text(), QStringLiteral("bob"));

    panel.setLoginOnline(QStringLiteral("bob"), false);
    EXPECT_EQ(panel.listWidget()->item(1)->text(), QStringLiteral("bob"));
}

TEST(MemberListPanelTest, SetOnlineLoginsReplacesThePreviousSetEntirely) {
    MemberListPanel panel;
    panel.setMembers({QStringLiteral("alice"), QStringLiteral("bob")});
    panel.setOnlineLogins({QStringLiteral("alice")});

    panel.setOnlineLogins({QStringLiteral("bob")});

    // Nothing observable from outside except that it doesn't crash and
    // item text is untouched — the icon itself isn't asserted on
    // (QIcon has no equality worth comparing), same limitation as the
    // rest of this file's icon-drawing methods.
    EXPECT_EQ(panel.listWidget()->item(0)->text(), QStringLiteral("alice"));
    EXPECT_EQ(panel.listWidget()->item(1)->text(), QStringLiteral("bob"));
}

TEST(MemberListPanelTest, ContextMenuEmitsNothingForTheCurrentUsersOwnRow) {
    MemberListPanel panel;
    panel.setCurrentUserLogin(QStringLiteral("alice"));
    panel.setChannelEncrypted(true);
    panel.setMembers({QStringLiteral("alice")});
    QSignalSpy spy(&panel, &MemberListPanel::grantChannelKeyAccessRequested);

    const QPoint pos = panel.listWidget()->visualItemRect(panel.listWidget()->item(0)).center();
    emit panel.listWidget()->customContextMenuRequested(pos);

    EXPECT_EQ(spy.count(), 0);
}

}  // namespace
}  // namespace devicehub
