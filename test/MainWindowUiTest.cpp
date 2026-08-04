#include "ui/MainWindow.h"

#include <gtest/gtest.h>

#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTabWidget>

#include "devices/DeviceEnumerator.h"

// These tests deliberately never click a button that would start real
// capture (mic/camera/screen): the plain devicehub_tests CLI binary has
// no Info.plist usage descriptions, and macOS aborts a process that
// touches those APIs without one — unlike DeviceHub.app. What's safe to
// verify without opening a device is that the UI builds correctly and
// its device lists match what DeviceEnumerator reports.

namespace devicehub {
namespace {

TEST(MainWindowUiTest, ConstructsWithoutCrashing) {
    const MainWindow window;
    EXPECT_EQ(window.windowTitle(), QStringLiteral("DeviceHub"));
}

TEST(MainWindowUiTest, DeviceSettingsAreSeparateTabsInDialog) {
    const MainWindow window;

    auto* tabs = window.findChild<QTabWidget*>("settingsTabs");
    ASSERT_NE(tabs, nullptr);
    EXPECT_EQ(tabs->count(), 4);
}

TEST(MainWindowUiTest, SidebarFooterAndAccountMenuExist) {
    const MainWindow window;

    auto* sidebar = window.findChild<QWidget*>("sidebar");
    auto* mainContentPlaceholder = window.findChild<QLabel*>("mainContentPlaceholder");
    auto* footerProfileLabel = window.findChild<QLabel*>("footerProfileLabel");
    auto* footerSettingsButton = window.findChild<QPushButton*>("footerSettingsButton");
    auto* accountMenuButton = window.findChild<QPushButton*>("accountMenuButton");

    ASSERT_NE(sidebar, nullptr);
    ASSERT_NE(mainContentPlaceholder, nullptr);
    ASSERT_NE(footerProfileLabel, nullptr);
    ASSERT_NE(footerSettingsButton, nullptr);
    ASSERT_NE(accountMenuButton, nullptr);

    EXPECT_EQ(footerProfileLabel->text(), QStringLiteral("Not signed in"));
}

TEST(MainWindowUiTest, DeviceCombosMatchEnumerator) {
    const MainWindow window;
    const DeviceEnumerator enumerator;

    auto* outputCombo = window.findChild<QComboBox*>("outputCombo");
    auto* inputCombo = window.findChild<QComboBox*>("inputCombo");
    auto* cameraCombo = window.findChild<QComboBox*>("cameraCombo");
    auto* screenCombo = window.findChild<QComboBox*>("screenCombo");

    ASSERT_NE(outputCombo, nullptr);
    ASSERT_NE(inputCombo, nullptr);
    ASSERT_NE(cameraCombo, nullptr);
    ASSERT_NE(screenCombo, nullptr);

    EXPECT_EQ(outputCombo->count(), enumerator.audioOutputs().size());
    EXPECT_EQ(inputCombo->count(), enumerator.audioInputs().size());
    EXPECT_EQ(cameraCombo->count(), enumerator.cameras().size());
    EXPECT_EQ(screenCombo->count(), enumerator.screens().size());
}

TEST(MainWindowUiTest, ActionControlsExistWithExpectedInitialLabels) {
    const MainWindow window;

    auto* playToneButton = window.findChild<QPushButton*>("playToneButton");
    auto* toggleMicButton = window.findChild<QPushButton*>("toggleMicButton");
    auto* toggleCameraButton = window.findChild<QPushButton*>("toggleCameraButton");
    auto* toggleScreenCaptureButton = window.findChild<QPushButton*>("toggleScreenCaptureButton");
    auto* requestTokenButton = window.findChild<QPushButton*>("requestTokenButton");

    ASSERT_NE(playToneButton, nullptr);
    ASSERT_NE(toggleMicButton, nullptr);
    ASSERT_NE(toggleCameraButton, nullptr);
    ASSERT_NE(toggleScreenCaptureButton, nullptr);
    ASSERT_NE(requestTokenButton, nullptr);

    EXPECT_EQ(playToneButton->text(), QStringLiteral("Play test tone"));
    EXPECT_EQ(toggleMicButton->text(), QStringLiteral("Start capture"));
    EXPECT_EQ(toggleCameraButton->text(), QStringLiteral("Start camera"));
    EXPECT_EQ(toggleScreenCaptureButton->text(), QStringLiteral("Start screen capture"));
    EXPECT_EQ(requestTokenButton->text(), QStringLiteral("Get token & verify"));
}

TEST(MainWindowUiTest, AuthFieldsExistWithPasswordMasked) {
    const MainWindow window;

    auto* loginEdit = window.findChild<QLineEdit*>("loginEdit");
    auto* passwordEdit = window.findChild<QLineEdit*>("passwordEdit");

    ASSERT_NE(loginEdit, nullptr);
    ASSERT_NE(passwordEdit, nullptr);
    EXPECT_EQ(passwordEdit->echoMode(), QLineEdit::Password);
}

TEST(MainWindowUiTest, DeviceErrorStatusLabelsExistAndStartEmpty) {
    const MainWindow window;

    auto* micStatusLabel = window.findChild<QLabel*>("micStatusLabel");
    auto* cameraStatusLabel = window.findChild<QLabel*>("cameraStatusLabel");
    auto* screenStatusLabel = window.findChild<QLabel*>("screenStatusLabel");

    ASSERT_NE(micStatusLabel, nullptr);
    ASSERT_NE(cameraStatusLabel, nullptr);
    ASSERT_NE(screenStatusLabel, nullptr);
    EXPECT_TRUE(micStatusLabel->text().isEmpty());
    EXPECT_TRUE(cameraStatusLabel->text().isEmpty());
    EXPECT_TRUE(screenStatusLabel->text().isEmpty());
}

TEST(MainWindowUiTest, ChatMessagingControlsExist) {
    const MainWindow window;

    auto* connectToChannelButton = window.findChild<QPushButton*>("connectToChannelButton");
    auto* chatLog = window.findChild<QPlainTextEdit*>("chatLog");
    auto* chatMessageEdit = window.findChild<QLineEdit*>("chatMessageEdit");
    auto* sendChatMessageButton = window.findChild<QPushButton*>("sendChatMessageButton");

    ASSERT_NE(connectToChannelButton, nullptr);
    ASSERT_NE(chatLog, nullptr);
    ASSERT_NE(chatMessageEdit, nullptr);
    ASSERT_NE(sendChatMessageButton, nullptr);

    EXPECT_TRUE(chatLog->isReadOnly());
    EXPECT_EQ(connectToChannelButton->text(), QStringLiteral("Connect to selected channel"));
    EXPECT_EQ(sendChatMessageButton->text(), QStringLiteral("Send"));
}

TEST(MainWindowUiTest, CommunityAndChannelManagementControlsExist) {
    const MainWindow window;

    auto* communityNameEdit = window.findChild<QLineEdit*>("communityNameEdit");
    auto* createCommunityButton = window.findChild<QPushButton*>("createCommunityButton");
    auto* communityCombo = window.findChild<QComboBox*>("communityCombo");
    auto* refreshCommunitiesButton = window.findChild<QPushButton*>("refreshCommunitiesButton");
    auto* joinCommunityButton = window.findChild<QPushButton*>("joinCommunityButton");
    auto* channelNameEdit = window.findChild<QLineEdit*>("channelNameEdit");
    auto* createChannelButton = window.findChild<QPushButton*>("createChannelButton");
    auto* channelCombo = window.findChild<QComboBox*>("channelCombo");
    auto* refreshChannelsButton = window.findChild<QPushButton*>("refreshChannelsButton");

    ASSERT_NE(communityNameEdit, nullptr);
    ASSERT_NE(createCommunityButton, nullptr);
    ASSERT_NE(communityCombo, nullptr);
    ASSERT_NE(refreshCommunitiesButton, nullptr);
    ASSERT_NE(joinCommunityButton, nullptr);
    ASSERT_NE(channelNameEdit, nullptr);
    ASSERT_NE(createChannelButton, nullptr);
    ASSERT_NE(channelCombo, nullptr);
    ASSERT_NE(refreshChannelsButton, nullptr);

    EXPECT_EQ(communityCombo->count(), 0);
    EXPECT_EQ(channelCombo->count(), 0);
    EXPECT_EQ(createCommunityButton->text(), QStringLiteral("Create community"));
    EXPECT_EQ(joinCommunityButton->text(), QStringLiteral("Join selected community"));
}

}  // namespace
}  // namespace devicehub
