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
