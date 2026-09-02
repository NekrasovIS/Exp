#include "ui/SettingsDialog.h"

#include <gtest/gtest.h>

#include <QComboBox>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QTabWidget>
#include <QVideoWidget>

namespace devicehub {
namespace {

// SettingsDialog is a "dumb widget" (see its class doc comment) — it owns
// no device objects and emits no signals of its own; MainWindow populates
// the combos and wires the buttons externally. These tests only cover the
// structural contract other code relies on: every getter resolves to a
// real widget with the objectName/initial state MainWindow expects to find.

TEST(SettingsDialogTest, HasFourTabsInTheExpectedOrder) {
    SettingsDialog dialog;

    auto* tabs = dialog.findChild<QTabWidget*>(QStringLiteral("settingsTabs"));
    ASSERT_NE(tabs, nullptr);
    ASSERT_EQ(tabs->count(), 4);
    EXPECT_EQ(tabs->tabText(0), QStringLiteral("Audio Output"));
    EXPECT_EQ(tabs->tabText(1), QStringLiteral("Microphone"));
    EXPECT_EQ(tabs->tabText(2), QStringLiteral("Camera"));
    EXPECT_EQ(tabs->tabText(3), QStringLiteral("Screen Capture"));
}

TEST(SettingsDialogTest, AudioOutputWidgetsExistAndAreEmptyByDefault) {
    SettingsDialog dialog;

    ASSERT_NE(dialog.outputCombo(), nullptr);
    EXPECT_EQ(dialog.outputCombo()->count(), 0);
    ASSERT_NE(dialog.playToneButton(), nullptr);
    EXPECT_EQ(dialog.playToneButton()->text(), QStringLiteral("Play test tone"));
}

TEST(SettingsDialogTest, MicrophoneWidgetsExistWithExpectedInitialState) {
    SettingsDialog dialog;

    ASSERT_NE(dialog.inputCombo(), nullptr);
    EXPECT_EQ(dialog.inputCombo()->count(), 0);
    ASSERT_NE(dialog.toggleMicButton(), nullptr);
    EXPECT_EQ(dialog.toggleMicButton()->text(), QStringLiteral("Start capture"));
    ASSERT_NE(dialog.micLevelBar(), nullptr);
    EXPECT_EQ(dialog.micLevelBar()->minimum(), 0);
    EXPECT_EQ(dialog.micLevelBar()->maximum(), 100);
    ASSERT_NE(dialog.micStatusLabel(), nullptr);
    EXPECT_TRUE(dialog.micStatusLabel()->text().isEmpty());
}

TEST(SettingsDialogTest, CameraWidgetsExistWithExpectedInitialState) {
    SettingsDialog dialog;

    ASSERT_NE(dialog.cameraCombo(), nullptr);
    EXPECT_EQ(dialog.cameraCombo()->count(), 0);
    ASSERT_NE(dialog.toggleCameraButton(), nullptr);
    EXPECT_EQ(dialog.toggleCameraButton()->text(), QStringLiteral("Start camera"));
    ASSERT_NE(dialog.videoPreview(), nullptr);
    ASSERT_NE(dialog.cameraStatusLabel(), nullptr);
    EXPECT_TRUE(dialog.cameraStatusLabel()->text().isEmpty());
}

TEST(SettingsDialogTest, ScreenCaptureWidgetsExistWithExpectedInitialState) {
    SettingsDialog dialog;

    ASSERT_NE(dialog.screenCombo(), nullptr);
    EXPECT_EQ(dialog.screenCombo()->count(), 0);
    ASSERT_NE(dialog.toggleScreenCaptureButton(), nullptr);
    EXPECT_EQ(dialog.toggleScreenCaptureButton()->text(), QStringLiteral("Start screen capture"));
    ASSERT_NE(dialog.screenPreview(), nullptr);
    ASSERT_NE(dialog.screenStatusLabel(), nullptr);
    EXPECT_TRUE(dialog.screenStatusLabel()->text().isEmpty());
}

TEST(SettingsDialogTest, WindowTitleIsSettings) {
    SettingsDialog dialog;

    EXPECT_EQ(dialog.windowTitle(), QStringLiteral("Settings"));
}

}  // namespace
}  // namespace devicehub
