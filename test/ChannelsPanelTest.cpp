#include "ui/ChannelsPanel.h"

#include <gtest/gtest.h>

#include <QLineEdit>
#include <QListWidget>
#include <QSignalSpy>

namespace devicehub {
namespace {

QList<ChatItem> sampleChannels() {
    return {
        ChatItem{.id = 10, .name = "general", .ownerLogin = "alice"},
        ChatItem{.id = 11, .name = "random", .ownerLogin = "bob"},
    };
}

TEST(ChannelsPanelTest, SetChannelsPopulatesTheListWidget) {
    ChannelsPanel panel;

    panel.setChannels(sampleChannels());

    ASSERT_EQ(panel.listWidget()->count(), 2);
    EXPECT_EQ(panel.listWidget()->item(0)->text(), QStringLiteral("general"));
    EXPECT_EQ(panel.listWidget()->item(1)->text(), QStringLiteral("random"));
}

TEST(ChannelsPanelTest, SetChannelsReplacesPreviousContents) {
    ChannelsPanel panel;
    panel.setChannels(sampleChannels());

    panel.setChannels({ChatItem{.id = 12, .name = "announcements", .ownerLogin = "carol"}});

    ASSERT_EQ(panel.listWidget()->count(), 1);
    EXPECT_EQ(panel.listWidget()->item(0)->text(), QStringLiteral("announcements"));
}

TEST(ChannelsPanelTest, SetChannelsWithEmptyListLeavesNoRows) {
    ChannelsPanel panel;
    panel.setChannels(sampleChannels());

    panel.setChannels({});

    EXPECT_EQ(panel.listWidget()->count(), 0);
}

TEST(ChannelsPanelTest, SelectChannelIdSelectsMatchingRowWithoutEmittingSignal) {
    ChannelsPanel panel;
    panel.setChannels(sampleChannels());
    QSignalSpy spy(&panel, &ChannelsPanel::channelSelected);

    panel.selectChannelId(11);

    EXPECT_EQ(panel.listWidget()->currentRow(), 1);
    EXPECT_EQ(spy.count(), 0);
}

TEST(ChannelsPanelTest, ClickingAnItemEmitsChannelSelectedWithIdAndName) {
    ChannelsPanel panel;
    panel.setChannels(sampleChannels());
    QSignalSpy spy(&panel, &ChannelsPanel::channelSelected);

    emit panel.listWidget()->itemClicked(panel.listWidget()->item(1));

    ASSERT_EQ(spy.count(), 1);
    EXPECT_EQ(spy.at(0).at(0).toLongLong(), 11);
    EXPECT_EQ(spy.at(0).at(1).toString(), QStringLiteral("random"));
}

TEST(ChannelsPanelTest, EncryptedChannelShowsLockPrefixInTextButRawNameInSelection) {
    ChannelsPanel panel;
    panel.setChannels({ChatItem{.id = 20, .name = "secrets", .ownerLogin = "alice", .isEncrypted = true}});
    QSignalSpy spy(&panel, &ChannelsPanel::channelSelected);

    EXPECT_TRUE(panel.listWidget()->item(0)->text().startsWith(QStringLiteral("\U0001F512")));
    EXPECT_TRUE(panel.listWidget()->item(0)->text().endsWith(QStringLiteral("secrets")));

    emit panel.listWidget()->itemClicked(panel.listWidget()->item(0));

    ASSERT_EQ(spy.count(), 1);
    EXPECT_EQ(spy.at(0).at(1).toString(), QStringLiteral("secrets"));
}

TEST(ChannelsPanelTest, FilterHidesNonMatchingRowsCaseInsensitively) {
    ChannelsPanel panel;
    panel.setChannels(sampleChannels());

    panel.filterEdit()->setText(QStringLiteral("GEN"));

    EXPECT_FALSE(panel.listWidget()->item(0)->isHidden());
    EXPECT_TRUE(panel.listWidget()->item(1)->isHidden());
}

TEST(ChannelsPanelTest, ClearingFilterShowsAllRowsAgain) {
    ChannelsPanel panel;
    panel.setChannels(sampleChannels());
    panel.filterEdit()->setText(QStringLiteral("gen"));

    panel.filterEdit()->clear();

    EXPECT_FALSE(panel.listWidget()->item(0)->isHidden());
    EXPECT_FALSE(panel.listWidget()->item(1)->isHidden());
}

TEST(ChannelsPanelTest, SetChannelsReappliesTheCurrentFilterToTheNewContents) {
    ChannelsPanel panel;
    panel.setChannels(sampleChannels());
    panel.filterEdit()->setText(QStringLiteral("gen"));

    panel.setChannels(
        {ChatItem{.id = 12, .name = "general-2", .ownerLogin = "carol"}, ChatItem{.id = 13, .name = "off-topic", .ownerLogin = "carol"}});

    EXPECT_FALSE(panel.listWidget()->item(0)->isHidden());
    EXPECT_TRUE(panel.listWidget()->item(1)->isHidden());
}

}  // namespace
}  // namespace devicehub
