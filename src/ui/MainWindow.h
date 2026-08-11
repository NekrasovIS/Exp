#pragma once

#include <QList>
#include <QMainWindow>
#include <memory>

#include "auth/AuthClient.h"
#include "chat/CallManager.h"
#include "chat/ChatClient.h"
#include "chat/ChatRestClient.h"
#include "devices/AudioInputDevice.h"
#include "devices/AudioOutputDevice.h"
#include "devices/CameraDevice.h"
#include "devices/DeviceEnumerator.h"
#include "devices/ScreenCaptureDevice.h"
#include "ui/ToastBanner.h"

class QScreen;

namespace devicehub {

class AccountMenu;
class ChannelsPanel;
class ChatView;
class CommunitiesPanel;
class FooterBar;
class SettingsDialog;

/**
 * @brief Main window shell: communities/channels sidebar on the left,
 *        the open channel's chat in the main area, an account menu
 *        top-right, and a footer with the profile and settings entry
 *        point.
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
    void onSendChatMessageClicked();
    void onCallToggleClicked();
    void onMuteToggleClicked();

    /// Re-lists communities from chat-service (no-op, with a status bar
    /// message, if not signed in yet).
    void refreshCommunities();
    /// Re-lists channels for selectedCommunityId_ (no-op, with a status
    /// bar message, if no community is selected).
    void refreshChannelsForSelectedCommunity();
    /// Switches ChatView to @p id/@p name, (re)connecting ChatClient.
    void openChannel(qint64 id, const QString& name);
    /// Drops the current channel selection/connection and shows
    /// ChatView's placeholder again.
    void closeChatView();
    /// CRUD feedback (create/rename/delete/join, errors) goes through
    /// this toast rather than statusBar() — much easier to notice.
    void showToast(const QString& text, ToastBanner::Variant variant);

    DeviceEnumerator enumerator_;
    AudioOutputDevice audioOutput_;
    AudioInputDevice audioInput_;
    CameraDevice camera_;
    ScreenCaptureDevice screenCapture_;
    AuthClient authClient_;
    ChatClient chatClient_;
    CallManager callManager_{chatClient_, audioInput_, audioOutput_};
    ChatRestClient chatRestClient_;
    QString lastToken_;
    QString currentUserLogin_;
    QList<QScreen*> screens_;
    QList<ChatItem> communities_;
    QList<ChatItem> channels_;
    QStringList callParticipants_;
    qint64 selectedCommunityId_ = -1;
    qint64 selectedChannelId_ = -1;
    qint64 pendingCommunitySelection_ = -1;
    qint64 pendingChannelSelection_ = -1;

    CommunitiesPanel* communitiesPanel_ = nullptr;
    ChannelsPanel* channelsPanel_ = nullptr;
    ChatView* chatView_ = nullptr;
    AccountMenu* accountMenu_ = nullptr;
    FooterBar* footerBar_ = nullptr;
    SettingsDialog* settingsDialog_ = nullptr;
    ToastBanner* toastBanner_ = nullptr;
};

}  // namespace devicehub
