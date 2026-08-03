#pragma once

#include <QList>
#include <QMainWindow>
#include <memory>

#include "auth/AuthClient.h"
#include "chat/ChatClient.h"
#include "chat/ChatRestClient.h"
#include "devices/AudioInputDevice.h"
#include "devices/AudioOutputDevice.h"
#include "devices/CameraDevice.h"
#include "devices/DeviceEnumerator.h"
#include "devices/ScreenCaptureDevice.h"

class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QScreen;
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
    void onToggleScreenCaptureClicked();
    void onRequestTokenClicked();
    void onConnectToChannelClicked();
    void onSendChatMessageClicked();
    void onCreateCommunityClicked();
    void onRefreshCommunitiesClicked();
    void onJoinCommunityClicked();
    void onCreateChannelClicked();
    void onRefreshChannelsClicked();

    DeviceEnumerator enumerator_;
    AudioOutputDevice audioOutput_;
    AudioInputDevice audioInput_;
    CameraDevice camera_;
    ScreenCaptureDevice screenCapture_;
    AuthClient authClient_;
    ChatClient chatClient_;
    ChatRestClient chatRestClient_;
    QString lastToken_;
    QList<QScreen*> screens_;
    QList<ChatItem> communities_;
    QList<ChatItem> channels_;

    QComboBox* outputCombo_ = nullptr;
    QComboBox* inputCombo_ = nullptr;
    QComboBox* cameraCombo_ = nullptr;
    QComboBox* screenCombo_ = nullptr;
    QPushButton* playToneButton_ = nullptr;
    QPushButton* toggleMicButton_ = nullptr;
    QPushButton* toggleCameraButton_ = nullptr;
    QPushButton* toggleScreenCaptureButton_ = nullptr;
    QPushButton* requestTokenButton_ = nullptr;
    QProgressBar* micLevelBar_ = nullptr;
    QVideoWidget* videoPreview_ = nullptr;
    QVideoWidget* screenPreview_ = nullptr;
    QLabel* cameraStatusLabel_ = nullptr;
    QLabel* screenStatusLabel_ = nullptr;
    QLineEdit* loginEdit_ = nullptr;
    QLineEdit* passwordEdit_ = nullptr;
    QLabel* authStatusLabel_ = nullptr;
    QLineEdit* communityNameEdit_ = nullptr;
    QPushButton* createCommunityButton_ = nullptr;
    QComboBox* communityCombo_ = nullptr;
    QPushButton* refreshCommunitiesButton_ = nullptr;
    QPushButton* joinCommunityButton_ = nullptr;
    QLineEdit* channelNameEdit_ = nullptr;
    QPushButton* createChannelButton_ = nullptr;
    QComboBox* channelCombo_ = nullptr;
    QPushButton* refreshChannelsButton_ = nullptr;
    QPushButton* connectToChannelButton_ = nullptr;
    QPlainTextEdit* chatLog_ = nullptr;
    QLineEdit* chatMessageEdit_ = nullptr;
    QPushButton* sendChatMessageButton_ = nullptr;
};

}  // namespace devicehub
