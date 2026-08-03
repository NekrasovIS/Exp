#pragma once

#include <QMainWindow>
#include <memory>

#include "devices/AudioInputDevice.h"
#include "devices/AudioOutputDevice.h"
#include "devices/CameraDevice.h"
#include "devices/DeviceEnumerator.h"

class QComboBox;
class QProgressBar;
class QPushButton;
class QVideoWidget;

namespace devicehub {

/**
 * @brief Main window: lets the user pick and exercise an audio output,
 *        a microphone, and a camera, one at a time.
 *
 * Pure presentation/wiring — all device access is delegated to the
 * devicehub::* classes in src/devices.
 */
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private:
    void buildUi();
    void populateDevices();
    void onPlayToneClicked();
    void onToggleMicClicked();
    void onToggleCameraClicked();

    DeviceEnumerator enumerator_;
    AudioOutputDevice audioOutput_;
    AudioInputDevice audioInput_;
    CameraDevice camera_;

    QComboBox* outputCombo_ = nullptr;
    QComboBox* inputCombo_ = nullptr;
    QComboBox* cameraCombo_ = nullptr;
    QPushButton* playToneButton_ = nullptr;
    QPushButton* toggleMicButton_ = nullptr;
    QPushButton* toggleCameraButton_ = nullptr;
    QProgressBar* micLevelBar_ = nullptr;
    QVideoWidget* videoPreview_ = nullptr;
};

}  // namespace devicehub
