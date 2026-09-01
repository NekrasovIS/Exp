#include "ui/MainWindow.h"

#include <gtest/gtest.h>

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMouseEvent>
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

TEST(MainWindowUiTest, AvatarClickShowsAccountMenuWithDisabledActionsWhenSignedOut) {
    // Issue #151: account settings move to a menu anchored on the
    // footer avatar rather than living only in the top-right
    // AccountMenu popup. Edit Profile/Sign Out only make sense once
    // signed in, so both start disabled here.
    const MainWindow window;

    auto* avatar = window.findChild<QLabel*>("footerAvatar");
    ASSERT_NE(avatar, nullptr);
    QMouseEvent press(QEvent::MouseButtonPress, QPointF(5, 5), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(avatar, &press);

    auto* menu = window.findChild<QMenu*>("accountSettingsMenu");
    ASSERT_NE(menu, nullptr);
    auto* editProfileAction = menu->findChild<QAction*>("editProfileAction");
    auto* signOutAction = menu->findChild<QAction*>("signOutAction");
    ASSERT_NE(editProfileAction, nullptr);
    ASSERT_NE(signOutAction, nullptr);
    EXPECT_FALSE(editProfileAction->isEnabled());
    EXPECT_FALSE(signOutAction->isEnabled());
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
    auto* registerButton = window.findChild<QPushButton*>("registerButton");

    ASSERT_NE(loginEdit, nullptr);
    ASSERT_NE(passwordEdit, nullptr);
    ASSERT_NE(registerButton, nullptr);
    EXPECT_EQ(passwordEdit->echoMode(), QLineEdit::Password);
    EXPECT_EQ(registerButton->text(), QStringLiteral("Register"));
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

    // #51: the flat QPlainTextEdit log was replaced by a scrollable,
    // dynamically-populated message list (ChatMessageRow widgets) — see
    // ChatMessageGroupingTest for the header-grouping logic itself.
    auto* chatMessagesContainer = window.findChild<QWidget*>("chatMessagesContainer");
    auto* chatMessageEdit = window.findChild<QLineEdit*>("chatMessageEdit");
    auto* sendChatMessageButton = window.findChild<QPushButton*>("sendChatMessageButton");
    auto* channelTitle = window.findChild<QLabel*>("chatChannelTitle");

    ASSERT_NE(chatMessagesContainer, nullptr);
    ASSERT_NE(chatMessageEdit, nullptr);
    ASSERT_NE(sendChatMessageButton, nullptr);
    ASSERT_NE(channelTitle, nullptr);

    EXPECT_EQ(sendChatMessageButton->text(), QStringLiteral("Send"));
}

TEST(MainWindowUiTest, AttachFileButtonExists) {
    // #116: "Attach" sits next to Send in ChatView's send row — see
    // MainWindow::onAttachFileClicked() for the upload-then-auto-send flow.
    const MainWindow window;

    auto* attachFileButton = window.findChild<QPushButton*>("attachFileButton");

    ASSERT_NE(attachFileButton, nullptr);
    EXPECT_EQ(attachFileButton->text(), QStringLiteral("Attach"));
}

TEST(MainWindowUiTest, CallControlsExistAndMuteStartsDisabled) {
    // #68: Call/Mute buttons live in ChatView's channel header — see
    // ChatView::setCallState() for how MainWindow keeps their
    // label/enabled state in sync with CallManager.
    const MainWindow window;

    auto* callToggleButton = window.findChild<QPushButton*>("callToggleButton");
    auto* muteToggleButton = window.findChild<QPushButton*>("muteToggleButton");

    ASSERT_NE(callToggleButton, nullptr);
    ASSERT_NE(muteToggleButton, nullptr);

    EXPECT_EQ(callToggleButton->text(), QStringLiteral("Call"));
    EXPECT_EQ(muteToggleButton->text(), QStringLiteral("Mute"));
    EXPECT_FALSE(muteToggleButton->isEnabled());
}

TEST(MainWindowUiTest, VideoToggleButtonExistsAndStartsDisabled) {
    // #91: video toggle lives in ChatView's channel header next to
    // Call/Mute — see ChatView::setVideoEnabled() for how MainWindow
    // keeps its label/enabled state in sync with CallManager.
    const MainWindow window;

    auto* videoToggleButton = window.findChild<QPushButton*>("videoToggleButton");

    ASSERT_NE(videoToggleButton, nullptr);
    EXPECT_EQ(videoToggleButton->text(), QStringLiteral("Enable Video"));
    EXPECT_FALSE(videoToggleButton->isEnabled());
}

TEST(MainWindowUiTest, ScreenShareToggleButtonExistsAndStartsDisabled) {
    // #112: mirrors the video toggle button — mutually exclusive with it
    // in CallManager, same UI-signal pattern.
    const MainWindow window;

    auto* screenShareToggleButton = window.findChild<QPushButton*>("screenShareToggleButton");

    ASSERT_NE(screenShareToggleButton, nullptr);
    EXPECT_EQ(screenShareToggleButton->text(), QStringLiteral("Share Screen"));
    EXPECT_FALSE(screenShareToggleButton->isEnabled());
}

TEST(MainWindowUiTest, SearchButtonExists) {
    // #118: "Search" lives in ChatView's channel header — see
    // MainWindow's SearchDialog wiring for the query/results flow.
    const MainWindow window;

    auto* searchButton = window.findChild<QPushButton*>("searchButton");

    ASSERT_NE(searchButton, nullptr);
    EXPECT_EQ(searchButton->text(), QStringLiteral("Search"));
}

TEST(MainWindowUiTest, CommunityAndChannelManagementControlsExist) {
    const MainWindow window;

    auto* communityList = window.findChild<QListWidget*>("communityList");
    auto* createCommunityButton = window.findChild<QPushButton*>("createCommunityButton");
    auto* refreshCommunitiesButton = window.findChild<QPushButton*>("refreshCommunitiesButton");
    auto* channelList = window.findChild<QListWidget*>("channelList");
    auto* createChannelButton = window.findChild<QPushButton*>("createChannelButton");
    auto* refreshChannelsButton = window.findChild<QPushButton*>("refreshChannelsButton");

    ASSERT_NE(communityList, nullptr);
    ASSERT_NE(createCommunityButton, nullptr);
    ASSERT_NE(refreshCommunitiesButton, nullptr);
    ASSERT_NE(channelList, nullptr);
    ASSERT_NE(createChannelButton, nullptr);
    ASSERT_NE(refreshChannelsButton, nullptr);

    EXPECT_EQ(communityList->count(), 0);
    EXPECT_EQ(channelList->count(), 0);
    EXPECT_FALSE(createCommunityButton->icon().isNull());
    EXPECT_FALSE(createChannelButton->icon().isNull());
}

TEST(MainWindowUiTest, EmptyStatesOfferAnActionButton) {
    const MainWindow window;

    // CommunitiesPanel is now a narrow icon rail (#50) with no room for
    // descriptive empty-state text — its ever-present "+" button is the
    // rail's own call to action, so only ChannelsPanel and ChatView's
    // placeholder still have a dedicated empty-state button.
    auto* channelEmptyStateButton = window.findChild<QPushButton*>("emptyStateCreateChannelButton");
    auto* placeholderCreateButton = window.findChild<QPushButton*>("placeholderCreateChannelButton");

    ASSERT_NE(channelEmptyStateButton, nullptr);
    ASSERT_NE(placeholderCreateButton, nullptr);

    EXPECT_EQ(channelEmptyStateButton->text(), QStringLiteral("Create channel"));
    EXPECT_EQ(placeholderCreateButton->text(), QStringLiteral("Create channel"));
}

}  // namespace
}  // namespace devicehub
