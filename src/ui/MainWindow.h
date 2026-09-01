#pragma once

#include <QHash>
#include <QList>
#include <QMainWindow>
#include <QSet>
#include <memory>
#include <optional>

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
#include "user/IdentityKeyStore.h"
#include "user/UserProfileClient.h"

class QScreen;
class QTimer;

namespace devicehub {

class AccountMenu;
class ChannelsPanel;
class ChatView;
class CommunitiesPanel;
class DesktopNotifier;
class FooterBar;
class LoginWindow;
class ModeratorsDialog;
class ProfileDialog;
class SearchDialog;
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
    /// "Attach" clicked (issue #116) — opens a file picker, then uploads
    /// the chosen file; the actual send happens once
    /// ChatRestClient::attachmentUploaded() fires (see MainWindow.cpp).
    void onAttachFileClicked();
    void onCallToggleClicked();
    void onMuteToggleClicked();
    void onVideoToggleClicked();
    void onEditProfileClicked();
    void onScreenShareToggleClicked();
    /// LoginWindow::requestCodeRequested() — issue #156.
    void onRequestOtpCodeClicked(const QString& identifier);
    /// LoginWindow::verifyCodeRequested() — issue #156.
    void onVerifyOtpCodeClicked(const QString& identifier, const QString& code);

    /// Re-lists communities from chat-service (no-op, with a status bar
    /// message, if not signed in yet).
    void refreshCommunities();
    /// Re-lists channels for selectedCommunityId_ (no-op, with a status
    /// bar message, if no community is selected).
    void refreshChannelsForSelectedCommunity();
    /// Switches ChatView to @p id/@p name, (re)connecting ChatClient. For
    /// an encrypted channel (issue #138) with no cached key yet, this
    /// fetches/unwraps the key first and defers subscribing/loading
    /// history to finishOpeningChannel() so nothing tries to decrypt
    /// before the key is available.
    void openChannel(qint64 id, const QString& name);
    /// Second half of openChannel() — subscribes ChatClient and loads
    /// history. Called immediately for a plaintext channel or one whose
    /// key is already cached; otherwise called from the
    /// myChannelKeyFetched()/myChannelKeyNotFound() handlers.
    void finishOpeningChannel(qint64 id);
    /// Drops the current channel selection/connection and shows
    /// ChatView's placeholder again.
    void closeChatView();
    /// One step of the pending encrypted-channel-creation flow (issue
    /// #138): if @p login is in pendingEncryptedSetup_->pendingMemberLogins,
    /// wraps the pending channel key for @p publicKeyBase64 (unless
    /// empty — that member hasn't published a key yet, so they're
    /// skipped with a toast) and publishes it via setChannelKey().
    /// A no-op if @p login isn't currently pending (i.e. this is an
    /// unrelated profileReceived(), e.g. the signed-in user's own).
    void wrapPendingEncryptedChannelKeyForMember(const QString& login, const QString& publicKeyBase64);
    /// Decrypts @p ciphertext with channelKeys_[selectedChannelId_] for
    /// display — a placeholder string (never the raw ciphertext) if no
    /// key is cached yet or decryption fails, so a decrypt failure reads
    /// as "can't decrypt" rather than showing garbled bytes.
    [[nodiscard]] QString decryptForDisplay(const QString& ciphertext) const;
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
    CallManager callManager_{chatClient_, audioInput_, audioOutput_, camera_, screenCapture_};
    ChatRestClient chatRestClient_;
    UserProfileClient userProfileClient_;
    /// Constructed once currentUserLogin_ is known (issue #136) — no
    /// default constructor, since a keypair is meaningless without a
    /// login to scope its storage file to.
    std::optional<IdentityKeyStore> identityKeyStore_;
    QString lastToken_;
    /// Long-lived token (issue #105) that refreshTimer_ redeems for a
    /// fresh lastToken_ shortly before it expires — empty when not
    /// signed in.
    QString refreshToken_;
    QTimer* refreshTimer_ = nullptr;
    QString currentUserLogin_;
    QList<QScreen*> screens_;
    QList<ChatItem> communities_;
    QList<ChatItem> channels_;
    QStringList callParticipants_;
    qint64 selectedCommunityId_ = -1;
    qint64 selectedChannelId_ = -1;
    qint64 pendingCommunitySelection_ = -1;
    qint64 pendingChannelSelection_ = -1;
    /// Id of the oldest message ChatView currently has for the open
    /// channel, or -1 if none loaded yet — the beforeId cursor for the
    /// next "Load older messages" fetch. Reset on every channel switch.
    qint64 oldestMessageId_ = -1;
    /// Filename to suggest in the save dialog once the matching
    /// ChatRestClient::attachmentDownloaded() reply arrives (issue #116)
    /// — keyed by attachment id since downloads can be in flight for more
    /// than one message at a time.
    QHash<qint64, QString> pendingDownloadFilenames_;

    /// Resolved (unwrapped) raw symmetric keys for encrypted channels
    /// (issue #138), keyed by channel id — session-only, never persisted.
    /// A missing entry for an encrypted channel means either the key
    /// hasn't been fetched/unwrapped yet, or none has been wrapped for
    /// this login (see myChannelKeyNotFound()).
    QHash<qint64, QByteArray> channelKeys_;
    /// True while selectedChannelId_ refers to an encrypted channel —
    /// gates encrypt-before-send/decrypt-before-display and disables
    /// Attach/Search (unsupported for encrypted channels this phase).
    bool currentChannelEncrypted_ = false;

    /// State for the multi-step "create an encrypted channel" flow:
    /// generate a key, then wrap+publish it for every community member
    /// who has already published a public key (issue #136). Valid only
    /// between channelCreated() for an encrypted channel and the last
    /// setChannelKey() call completing.
    struct PendingEncryptedChannelSetup {
        qint64 channelId = -1;
        QByteArray channelKey;
        /// Logins still waiting on a fetchProfile() reply to learn their
        /// public key before they can be wrapped for.
        QSet<QString> pendingMemberLogins;
    };
    std::optional<PendingEncryptedChannelSetup> pendingEncryptedSetup_;

    CommunitiesPanel* communitiesPanel_ = nullptr;
    ChannelsPanel* channelsPanel_ = nullptr;
    ChatView* chatView_ = nullptr;
    AccountMenu* accountMenu_ = nullptr;
    FooterBar* footerBar_ = nullptr;
    SettingsDialog* settingsDialog_ = nullptr;
    ModeratorsDialog* moderatorsDialog_ = nullptr;
    ProfileDialog* profileDialog_ = nullptr;
    SearchDialog* searchDialog_ = nullptr;
    LoginWindow* loginWindow_ = nullptr;
    ToastBanner* toastBanner_ = nullptr;
    DesktopNotifier* desktopNotifier_ = nullptr;
};

}  // namespace devicehub
