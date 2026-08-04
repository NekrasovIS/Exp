#pragma once

#include <QDialog>

class QComboBox;
class QLabel;
class QProgressBar;
class QPushButton;
class QVideoWidget;

namespace devicehub {

/**
 * @brief Device settings: audio output, microphone, camera, screen
 *        capture — one tab each, opened from FooterBar's gear button.
 *
 * Pure presentation (identical controls to the old top-level tabs, just
 * relocated into a dialog) — MainWindow still owns the device objects
 * and all the wiring.
 */
class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget* parent = nullptr);

    [[nodiscard]] QComboBox* outputCombo() const { return outputCombo_; }
    [[nodiscard]] QPushButton* playToneButton() const { return playToneButton_; }

    [[nodiscard]] QComboBox* inputCombo() const { return inputCombo_; }
    [[nodiscard]] QPushButton* toggleMicButton() const { return toggleMicButton_; }
    [[nodiscard]] QProgressBar* micLevelBar() const { return micLevelBar_; }
    [[nodiscard]] QLabel* micStatusLabel() const { return micStatusLabel_; }

    [[nodiscard]] QComboBox* cameraCombo() const { return cameraCombo_; }
    [[nodiscard]] QPushButton* toggleCameraButton() const { return toggleCameraButton_; }
    [[nodiscard]] QVideoWidget* videoPreview() const { return videoPreview_; }
    [[nodiscard]] QLabel* cameraStatusLabel() const { return cameraStatusLabel_; }

    [[nodiscard]] QComboBox* screenCombo() const { return screenCombo_; }
    [[nodiscard]] QPushButton* toggleScreenCaptureButton() const { return toggleScreenCaptureButton_; }
    [[nodiscard]] QVideoWidget* screenPreview() const { return screenPreview_; }
    [[nodiscard]] QLabel* screenStatusLabel() const { return screenStatusLabel_; }

private:
    QComboBox* outputCombo_ = nullptr;
    QPushButton* playToneButton_ = nullptr;

    QComboBox* inputCombo_ = nullptr;
    QPushButton* toggleMicButton_ = nullptr;
    QProgressBar* micLevelBar_ = nullptr;
    QLabel* micStatusLabel_ = nullptr;

    QComboBox* cameraCombo_ = nullptr;
    QPushButton* toggleCameraButton_ = nullptr;
    QVideoWidget* videoPreview_ = nullptr;
    QLabel* cameraStatusLabel_ = nullptr;

    QComboBox* screenCombo_ = nullptr;
    QPushButton* toggleScreenCaptureButton_ = nullptr;
    QVideoWidget* screenPreview_ = nullptr;
    QLabel* screenStatusLabel_ = nullptr;
};

}  // namespace devicehub
