#include "ui/FriendsPanel.h"

#include <gtest/gtest.h>

#include <QListWidget>
#include <QSignalSpy>
#include <QWidget>

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

// issue #216: FriendsPanel больше не заменяет ChannelsPanel в общей
// раскладке (и с ней не убирается кнопка "< Communities" — панель
// всплывает поверх своего родителя, закрывается тем же переключателем
// "Friends", что её открыл, см. MainWindow::onFriendsButtonClicked()).
TEST(FriendsPanelTest, SetOpenTogglesIsOpen) {
    QWidget host;
    host.resize(240, 600);
    FriendsPanel panel(&host);

    EXPECT_FALSE(panel.isOpen());

    panel.setOpen(true);
    EXPECT_TRUE(panel.isOpen());

    panel.setOpen(false);
    EXPECT_FALSE(panel.isOpen());
}

// Без родителя-виджета (как во всех остальных тестах этого файла) панель
// не может ничего перекрывать собой — setOpen() безопасно ничего не
// делает, а не падает на nullptr parentWidget().
TEST(FriendsPanelTest, SetOpenWithoutAParentIsANoOp) {
    FriendsPanel panel;

    panel.setOpen(true);

    EXPECT_FALSE(panel.isOpen());
}

}  // namespace
}  // namespace devicehub
