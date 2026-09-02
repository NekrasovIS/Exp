#include "ui/MemberListPanel.h"

#include <gtest/gtest.h>

#include <QLabel>
#include <QListWidget>

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

}  // namespace
}  // namespace devicehub
