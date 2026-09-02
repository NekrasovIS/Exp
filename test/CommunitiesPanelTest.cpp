#include "ui/CommunitiesPanel.h"

#include <gtest/gtest.h>

#include <QApplication>
#include <QDialog>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSignalSpy>
#include <QTimer>

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

TEST(CommunitiesPanelTest, ClickingAddButtonOpensDialogAndEmitsCreateRequestedWithEnteredName) {
    CommunitiesPanel panel;
    QSignalSpy spy(&panel, &CommunitiesPanel::createRequested);

    // showAddDialog() uses QInputDialog::getText(), which blocks in its
    // own exec() — same reason ChannelsPanelTest reaches into a dialog
    // via QApplication::activeModalWidget() once its event loop is
    // already spinning, not by calling the dialog's own code directly.
    // QInputDialog's text-entry mode always builds exactly one QLineEdit,
    // so a plain (unnamed) findChild is enough to reach it.
    QTimer::singleShot(0, &panel, []() {
        auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
        ASSERT_NE(dialog, nullptr);
        auto* nameEdit = dialog->findChild<QLineEdit*>();
        ASSERT_NE(nameEdit, nullptr);
        nameEdit->setText(QStringLiteral("New Community"));
        dialog->accept();
    });

    panel.addButton()->click();

    ASSERT_EQ(spy.count(), 1);
    EXPECT_EQ(spy.at(0).at(0).toString(), QStringLiteral("New Community"));
}

TEST(CommunitiesPanelTest, RefreshButtonExists) {
    CommunitiesPanel panel;

    ASSERT_NE(panel.refreshButton(), nullptr);
    EXPECT_TRUE(panel.refreshButton()->isEnabled());
}

}  // namespace
}  // namespace devicehub
