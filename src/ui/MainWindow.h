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

class QScreen;

namespace devicehub {

class AccountMenu;
class ChatPanel;
class CommunitiesPanel;
class FooterBar;
class SettingsDialog;

/**
 * @brief Main window shell: communities/chat sidebar on the left, an
 *        account menu top-right, and a footer with the profile and
 *        settings entry point.
 *
 * Pure presentation/wiring — all device and network access is delegated
 * to the devicehub::* classes in src/devices, src/auth and src/chat; all
 * widget construction is delegated to the panel classes in src/ui.
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
    void onRegisterClicked();
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
    qint64 pendingCommunitySelection_ = -1;
    qint64 pendingChannelSelection_ = -1;

    CommunitiesPanel* communitiesPanel_ = nullptr;
    ChatPanel* chatPanel_ = nullptr;
    AccountMenu* accountMenu_ = nullptr;
    FooterBar* footerBar_ = nullptr;
    SettingsDialog* settingsDialog_ = nullptr;
};

}  // namespace devicehub
