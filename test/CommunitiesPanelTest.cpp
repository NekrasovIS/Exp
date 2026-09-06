#include "ui/CommunitiesPanel.h"

#include <gtest/gtest.h>

#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSignalSpy>

#include "ui/CommunityConnectDialog.h"

namespace devicehub {
namespace {

QList<ChatItem> sampleCommunities() {
    return {
        ChatItem{.id = 1, .name = "Alpha", .ownerLogin = "alice"},
        ChatItem{.id = 2, .name = "Beta", .ownerLogin = "bob"},
    };
}

TEST(CommunitiesPanelTest, SetCommunitiesPopulatesTheListWidget) {
    CommunitiesPanel panel;

    panel.setCommunities(sampleCommunities());

    ASSERT_EQ(panel.listWidget()->count(), 2);
    EXPECT_EQ(panel.listWidget()->item(0)->toolTip(), QStringLiteral("Alpha"));
    EXPECT_EQ(panel.listWidget()->item(1)->toolTip(), QStringLiteral("Beta"));
}

TEST(CommunitiesPanelTest, SetCommunitiesReplacesPreviousContents) {
    CommunitiesPanel panel;
    panel.setCommunities(sampleCommunities());

    panel.setCommunities({ChatItem{.id = 3, .name = "Gamma", .ownerLogin = "carol"}});

    ASSERT_EQ(panel.listWidget()->count(), 1);
    EXPECT_EQ(panel.listWidget()->item(0)->toolTip(), QStringLiteral("Gamma"));
}

TEST(CommunitiesPanelTest, SelectCommunityIdSelectsMatchingRowWithoutEmittingSignal) {
    CommunitiesPanel panel;
    panel.setCommunities(sampleCommunities());
    QSignalSpy spy(&panel, &CommunitiesPanel::communitySelected);

    panel.selectCommunityId(2);

    EXPECT_EQ(panel.listWidget()->currentRow(), 1);
    EXPECT_EQ(spy.count(), 0);
}

TEST(CommunitiesPanelTest, SelectCommunityIdWithUnknownIdSelectsNothing) {
    CommunitiesPanel panel;
    panel.setCommunities(sampleCommunities());

    panel.selectCommunityId(999);

    EXPECT_EQ(panel.listWidget()->currentRow(), -1);
}

TEST(CommunitiesPanelTest, ClickingAnItemEmitsCommunitySelected) {
    CommunitiesPanel panel;
    panel.setCommunities(sampleCommunities());
    QSignalSpy spy(&panel, &CommunitiesPanel::communitySelected);

    emit panel.listWidget()->itemClicked(panel.listWidget()->item(1));

    ASSERT_EQ(spy.count(), 1);
    EXPECT_EQ(spy.at(0).at(0).toLongLong(), 2);
}

TEST(CommunitiesPanelTest, ClickingAddButtonShowsTheConnectDialog) {
    CommunitiesPanel panel;
    ASSERT_NE(panel.connectDialog(), nullptr);
    ASSERT_TRUE(panel.connectDialog()->isHidden());

    panel.addButton()->click();

    EXPECT_FALSE(panel.connectDialog()->isHidden());
}

TEST(CommunitiesPanelTest, JoiningThroughTheConnectDialogEmitsJoinByCodeRequested) {
    // issue #186: подключение по коду приглашения, не по id из общего
    // списка — CommunitiesPanel ретранслирует сигнал connectDialog()
    // под своим собственным именем.
    CommunitiesPanel panel;
    QSignalSpy spy(&panel, &CommunitiesPanel::joinByCodeRequested);

    panel.connectDialog()->inviteCodeEdit()->setText(QStringLiteral("ABC123"));
    panel.connectDialog()->joinButton()->click();

    ASSERT_EQ(spy.count(), 1);
    EXPECT_EQ(spy.at(0).at(0).toString(), QStringLiteral("ABC123"));
}

TEST(CommunitiesPanelTest, CreatingThroughTheConnectDialogEmitsCreateRequested) {
    CommunitiesPanel panel;
    QSignalSpy spy(&panel, &CommunitiesPanel::createRequested);

    panel.connectDialog()->nameEdit()->setText(QStringLiteral("New Community"));
    panel.connectDialog()->createButton()->click();

    ASSERT_EQ(spy.count(), 1);
    EXPECT_EQ(spy.at(0).at(0).toString(), QStringLiteral("New Community"));
}

}  // namespace
}  // namespace devicehub
