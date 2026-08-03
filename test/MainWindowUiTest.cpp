#include "ui/MainWindow.h"

#include <gtest/gtest.h>

#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>

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

}  // namespace
}  // namespace devicehub
