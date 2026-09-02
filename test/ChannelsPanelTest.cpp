#include "ui/ChannelsPanel.h"

#include <gtest/gtest.h>

#include <QApplication>
#include <QCheckBox>
#include <QDateTime>
#include <QDialog>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSignalSpy>
#include <QTimer>

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

TEST(ChannelsPanelTest, AddDialogNameEditAndEncryptedCheckBoxRoundTripIntoCreateRequested) {
    ChannelsPanel panel;
    QSignalSpy spy(&panel, &ChannelsPanel::createRequested);

    // showAddDialog() builds its QDialog on the stack and blocks in
    // exec() — the only way to reach newChannelNameEdit/
    // newChannelEncryptedCheckBox from outside is to catch the dialog
    // once its event loop is already spinning (QApplication::
    // activeModalWidget()), same pattern Qt itself recommends for
    // testing a modal exec() call.
    QTimer::singleShot(0, &panel, []() {
        auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
        ASSERT_NE(dialog, nullptr);

        auto* nameEdit = dialog->findChild<QLineEdit*>(QStringLiteral("newChannelNameEdit"));
        ASSERT_NE(nameEdit, nullptr);
        auto* encryptedCheckBox = dialog->findChild<QCheckBox*>(QStringLiteral("newChannelEncryptedCheckBox"));
        ASSERT_NE(encryptedCheckBox, nullptr);
        EXPECT_FALSE(encryptedCheckBox->isChecked());

        nameEdit->setText(QStringLiteral("secret-plans"));
        encryptedCheckBox->setChecked(true);
        dialog->accept();
    });

    panel.addButton()->click();

    ASSERT_EQ(spy.count(), 1);
    EXPECT_EQ(spy.at(0).at(0).toString(), QStringLiteral("secret-plans"));
    EXPECT_TRUE(spy.at(0).at(1).toBool());
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

TEST(ChannelsPanelTest, RecordChannelActivityAddsPreviewAsSecondLine) {
    ChannelsPanel panel;
    panel.setChannels(sampleChannels());

    panel.recordChannelActivity(10, 1, QStringLiteral("hey there"), QDateTime::currentDateTime());

    QListWidgetItem* item = panel.listWidget()->item(0);
    ASSERT_EQ(item->data(Qt::UserRole).toLongLong(), 10);
    EXPECT_EQ(item->text(), QStringLiteral("general\nhey there"));
}

TEST(ChannelsPanelTest, RecordChannelActivitySortsChannelsWithActivityFirst) {
    ChannelsPanel panel;
    panel.setChannels(sampleChannels());  // id 10 "general" first, id 11 "random" second.

    // "random" (id 11) gets activity — should float to the top even
    // though it was listed second by the server.
    panel.recordChannelActivity(11, 1, QStringLiteral("ping"), QDateTime::currentDateTime());

    ASSERT_EQ(panel.listWidget()->count(), 2);
    EXPECT_EQ(panel.listWidget()->item(0)->data(Qt::UserRole).toLongLong(), 11);
    EXPECT_EQ(panel.listWidget()->item(1)->data(Qt::UserRole).toLongLong(), 10);
}

TEST(ChannelsPanelTest, MoreRecentActivitySortsAboveOlderActivity) {
    ChannelsPanel panel;
    panel.setChannels(sampleChannels());
    const QDateTime now = QDateTime::currentDateTime();

    panel.recordChannelActivity(10, 1, QStringLiteral("older"), now.addSecs(-60));
    panel.recordChannelActivity(11, 2, QStringLiteral("newer"), now);

    ASSERT_EQ(panel.listWidget()->count(), 2);
    EXPECT_EQ(panel.listWidget()->item(0)->data(Qt::UserRole).toLongLong(), 11);
    EXPECT_EQ(panel.listWidget()->item(1)->data(Qt::UserRole).toLongLong(), 10);
}

TEST(ChannelsPanelTest, RecordChannelActivityMarksTheRowBoldForAChannelThatIsNotOpen) {
    ChannelsPanel panel;
    panel.setChannels(sampleChannels());
    ASSERT_FALSE(panel.listWidget()->item(0)->font().bold());

    panel.recordChannelActivity(10, 1, QStringLiteral("new message"), QDateTime::currentDateTime());

    EXPECT_TRUE(panel.listWidget()->item(0)->font().bold());
}

TEST(ChannelsPanelTest, ActivityForTheCurrentlyOpenChannelIsNeverMarkedUnread) {
    ChannelsPanel panel;
    panel.setChannels(sampleChannels());
    panel.setOpenChannelId(10);

    panel.recordChannelActivity(10, 1, QStringLiteral("I'm looking at this"), QDateTime::currentDateTime());

    EXPECT_FALSE(panel.listWidget()->item(0)->font().bold());
}

TEST(ChannelsPanelTest, OpeningAnAlreadyUnreadChannelClearsTheBoldIndicator) {
    ChannelsPanel panel;
    panel.setChannels(sampleChannels());
    panel.recordChannelActivity(10, 1, QStringLiteral("new message"), QDateTime::currentDateTime());
    ASSERT_TRUE(panel.listWidget()->item(0)->font().bold());

    panel.setOpenChannelId(10);

    EXPECT_FALSE(panel.listWidget()->item(0)->font().bold());
}

TEST(ChannelsPanelTest, ANewerMessageArrivingAfterTheChannelWasReadIsMarkedUnreadAgain) {
    ChannelsPanel panel;
    panel.setChannels(sampleChannels());
    panel.recordChannelActivity(10, 1, QStringLiteral("first"), QDateTime::currentDateTime());
    panel.setOpenChannelId(10);
    panel.setOpenChannelId(-1);  // Leave the channel — id 1 is now the "read" watermark.
    ASSERT_FALSE(panel.listWidget()->item(0)->font().bold());

    panel.recordChannelActivity(10, 2, QStringLiteral("second"), QDateTime::currentDateTime());

    EXPECT_TRUE(panel.listWidget()->item(0)->font().bold());
}

TEST(ChannelsPanelTest, RefreshButtonExists) {
    ChannelsPanel panel;

    ASSERT_NE(panel.refreshButton(), nullptr);
    EXPECT_TRUE(panel.refreshButton()->isEnabled());
}

}  // namespace
}  // namespace devicehub
