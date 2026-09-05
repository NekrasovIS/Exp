#include "ui/FriendsPanel.h"

#include <gtest/gtest.h>

#include <QListWidget>
#include <QSignalSpy>

namespace devicehub {
namespace {

TEST(FriendsPanelTest, SetFriendsPopulatesTheList) {
    FriendsPanel panel;

    panel.setFriends({"alice", "bob"});

    ASSERT_EQ(panel.friendsList()->count(), 2);
    EXPECT_EQ(panel.friendsList()->item(0)->text(), QStringLiteral("alice"));
    EXPECT_EQ(panel.friendsList()->item(1)->text(), QStringLiteral("bob"));
}

TEST(FriendsPanelTest, SetFriendsReplacesPreviousContents) {
    FriendsPanel panel;
    panel.setFriends({"alice", "bob"});

    panel.setFriends({"carol"});

    ASSERT_EQ(panel.friendsList()->count(), 1);
    EXPECT_EQ(panel.friendsList()->item(0)->text(), QStringLiteral("carol"));
}

TEST(FriendsPanelTest, ClickingAFriendEmitsFriendSelected) {
    FriendsPanel panel;
    panel.setFriends({"alice", "bob"});
    QSignalSpy spy(&panel, &FriendsPanel::friendSelected);

    emit panel.friendsList()->itemClicked(panel.friendsList()->item(1));

    ASSERT_EQ(spy.count(), 1);
    EXPECT_EQ(spy.at(0).at(0).toString(), QStringLiteral("bob"));
}

TEST(FriendsPanelTest, SetIncomingRequestsPopulatesTheList) {
    FriendsPanel panel;

    panel.setIncomingRequests({FriendRequestInfo{.id = 1, .requesterLogin = "alice", .createdAt = "now"},
                                FriendRequestInfo{.id = 2, .requesterLogin = "bob", .createdAt = "now"}});

    ASSERT_EQ(panel.requestsList()->count(), 2);
    EXPECT_TRUE(panel.requestsList()->item(0)->text().contains(QStringLiteral("alice")));
    EXPECT_TRUE(panel.requestsList()->item(1)->text().contains(QStringLiteral("bob")));
}

TEST(FriendsPanelTest, SetIncomingRequestsReplacesPreviousContents) {
    FriendsPanel panel;
    panel.setIncomingRequests({FriendRequestInfo{.id = 1, .requesterLogin = "alice", .createdAt = "now"}});

    panel.setIncomingRequests({});

    EXPECT_EQ(panel.requestsList()->count(), 0);
}

}  // namespace
}  // namespace devicehub
