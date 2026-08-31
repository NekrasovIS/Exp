#include "ui/MainWindow.h"

#include <QComboBox>
#include <QDateTime>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QScreen>
#include <QTimer>
#include <QVBoxLayout>
#include <QVideoWidget>
#include <QWidget>

#include <algorithm>
#include <utility>

#include "ui/AccountMenu.h"
#include "ui/ChannelsPanel.h"
#include "ui/ChatMessageRow.h"
#include "ui/ChatView.h"
#include "ui/CommunitiesPanel.h"
#include "ui/DesktopNotifier.h"
#include "ui/FooterBar.h"
#include "ui/ProfileDialog.h"
#include "ui/SettingsDialog.h"
#include "ui/Theme.h"

namespace devicehub {

namespace {
constexpr const char* kDefaultAuthServiceUrl = "http://127.0.0.1:8080";
constexpr const char* kDefaultChatServiceWsUrl = "ws://127.0.0.1:8083";
constexpr const char* kDefaultChatServiceUrl = "http://127.0.0.1:8082";
constexpr const char* kDefaultUserServiceUrl = "http://127.0.0.1:8081";
constexpr int kToastTimeoutMs = 4000;
/// Redeem the refresh token this long before the access token actually
/// expires (issue #105) — a little slack so a refresh in flight has
/// time to land before anything using lastToken_ would start getting
/// 401s.
constexpr qint64 kRefreshBufferSeconds = 60;
constexpr int kMessagePageSize = 50;
}  // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent),
      authClient_(QUrl(qEnvironmentVariable("AUTH_SERVICE_URL", kDefaultAuthServiceUrl))),
      chatClient_(QUrl(qEnvironmentVariable("CHAT_SERVICE_WS_URL", kDefaultChatServiceWsUrl))),
      chatRestClient_(QUrl(qEnvironmentVariable("CHAT_SERVICE_URL", kDefaultChatServiceUrl))),
      userProfileClient_(QUrl(qEnvironmentVariable("USER_SERVICE_URL", kDefaultUserServiceUrl))) {
    buildUi();
    populateDevices();

    refreshTimer_ = new QTimer(this);
    refreshTimer_->setSingleShot(true);
    connect(refreshTimer_, &QTimer::timeout, this, [this]() {
        if (!refreshToken_.isEmpty()) {
            authClient_.refreshAccessToken(refreshToken_);
        }
    });

    connect(settingsDialog_->playToneButton(), &QPushButton::clicked, this, &MainWindow::onPlayToneClicked);
    connect(settingsDialog_->toggleMicButton(), &QPushButton::clicked, this, &MainWindow::onToggleMicClicked);
    connect(settingsDialog_->toggleCameraButton(), &QPushButton::clicked, this, &MainWindow::onToggleCameraClicked);
    connect(settingsDialog_->toggleScreenCaptureButton(), &QPushButton::clicked, this,
            &MainWindow::onToggleScreenCaptureClicked);
    connect(accountMenu_->requestTokenButton(), &QPushButton::clicked, this, &MainWindow::onRequestTokenClicked);
    connect(accountMenu_->registerButton(), &QPushButton::clicked, this, &MainWindow::onRegisterClicked);
    connect(accountMenu_->editProfileButton(), &QPushButton::clicked, this, &MainWindow::onEditProfileClicked);
    connect(profileDialog_, &ProfileDialog::saveRequested, this, [this](const QString& displayName, const QString& avatarUrl) {
        userProfileClient_.updateOwnProfile(lastToken_, displayName, avatarUrl);
    });
    connect(&userProfileClient_, &UserProfileClient::profileReceived, this, [this](const UserProfile& profile) {
        footerBar_->setProfileText(profile.displayName.isEmpty() ? currentUserLogin_ : profile.displayName);
        profileDialog_->setProfile(profile);
    });
    connect(&userProfileClient_, &UserProfileClient::profileUpdated, this, [this](const UserProfile& profile) {
        footerBar_->setProfileText(profile.displayName.isEmpty() ? currentUserLogin_ : profile.displayName);
        profileDialog_->setProfile(profile);
        profileDialog_->statusLabel()->setText(tr("Saved"));
    });
    connect(&userProfileClient_, &UserProfileClient::errorOccurred, this, [this](const QString& message) {
        profileDialog_->statusLabel()->setText(tr("Error: %1").arg(message));
    });
    connect(&audioInput_, &AudioInputDevice::levelChanged, settingsDialog_->micLevelBar(), [this](float level) {
        settingsDialog_->micLevelBar()->setValue(static_cast<int>(level * 100.0f));
    });
    connect(&audioInput_, &AudioInputDevice::errorOccurred, this, [this](const QString& message) {
        settingsDialog_->micStatusLabel()->setText(tr("Error: %1").arg(message));
    });
    connect(&camera_, &CameraDevice::errorOccurred, this, [this](const QString& message) {
        settingsDialog_->cameraStatusLabel()->setText(tr("Error: %1").arg(message));
    });
    connect(&screenCapture_, &ScreenCaptureDevice::errorOccurred, this, [this](const QString& message) {
        settingsDialog_->screenStatusLabel()->setText(tr("Error: %1").arg(message));
    });
    connect(&authClient_, &AuthClient::tokenReceived, this,
            [this](const QString& token, const QString& refreshToken, qint64 expiresAt) {
                lastToken_ = token;
                refreshToken_ = refreshToken;
                accountMenu_->statusLabel()->setText(tr("Token received, verifying..."));
                authClient_.verifyToken(token);

                // Silently redeem the refresh token shortly before this
                // access token expires, rather than waiting for a 401
                // and forcing the user to log in again mid-session.
                if (!refreshToken_.isEmpty()) {
                    const qint64 nowSecs = QDateTime::currentSecsSinceEpoch();
                    const qint64 delaySecs = std::max<qint64>(1, expiresAt - nowSecs - kRefreshBufferSeconds);
                    refreshTimer_->start(static_cast<int>(std::min<qint64>(delaySecs, 24 * 3600)) * 1000);
                }
            });
    connect(&authClient_, &AuthClient::tokenVerified, this, [this](bool valid, const QString& subject) {
        accountMenu_->statusLabel()->setText(valid ? tr("Verified — subject: %1").arg(subject) : tr("Token rejected"));
        currentUserLogin_ = valid ? subject : QString();
        footerBar_->setProfileText(valid ? subject : tr("Not signed in"));
        communitiesPanel_->setCurrentUserLogin(currentUserLogin_);
        channelsPanel_->setCurrentUserLogin(currentUserLogin_);
        chatView_->setCurrentUserLogin(currentUserLogin_);
        accountMenu_->setEditProfileEnabled(valid);
        if (valid) {
            refreshCommunities();
            // Populates the footer's display name (falls back to the
            // bare login above until this returns) and prefills
            // ProfileDialog for whenever Edit Profile is clicked.
            userProfileClient_.fetchProfile(lastToken_, currentUserLogin_);
        }
    });
    connect(&authClient_, &AuthClient::errorOccurred, this, [this](const QString& message) {
        accountMenu_->statusLabel()->setText(tr("Error: %1").arg(message));
    });
    connect(&authClient_, &AuthClient::registrationCompleted, this, [this](bool registered) {
        if (!registered) {
            accountMenu_->statusLabel()->setText(tr("Registration failed — login already taken"));
        }
        // On success, tokenReceived() (auto-login) fires right after this
        // and takes the status label the rest of the way to "Verified".
    });

    connect(chatView_->sendButton(), &QPushButton::clicked, this, &MainWindow::onSendChatMessageClicked);
    connect(&chatClient_, &ChatClient::subscribed, this,
            [this](qint64 channelId) { chatView_->appendSystemLine(tr("-- subscribed to channel %1 --").arg(channelId)); });
    connect(&chatClient_, &ChatClient::messageReceived, this,
            [this](qint64 id, const QString& author, const QString& body, const QString& sentAt) {
                chatView_->appendMessage(ChatMessage{.id = id, .author = author, .body = body, .sentAt = sentAt});
                desktopNotifier_->notifyMessage(author, body, currentUserLogin_);
            });
    connect(&chatClient_, &ChatClient::messageEdited, this,
            [this](qint64 id, const QString& newBody, const QString& /*editedAt*/) {
                chatView_->updateMessageBody(id, newBody);
            });
    connect(&chatClient_, &ChatClient::messageDeleted, this,
            [this](qint64 id) { chatView_->removeMessage(id); });
    connect(&chatClient_, &ChatClient::errorOccurred, this,
            [this](const QString& message) { chatView_->appendSystemLine(tr("-- error: %1 --").arg(message)); });
    connect(chatView_, &ChatView::typingRequested, this, [this]() { chatClient_.sendTyping(); });
    connect(&chatClient_, &ChatClient::userTyping, this,
            [this](const QString& login) { chatView_->showTypingUser(login); });

    connect(chatView_, &ChatView::callToggleRequested, this, &MainWindow::onCallToggleClicked);
    connect(chatView_, &ChatView::muteToggleRequested, this, &MainWindow::onMuteToggleClicked);
    connect(chatView_, &ChatView::videoToggleRequested, this, &MainWindow::onVideoToggleClicked);
    connect(chatView_, &ChatView::screenShareToggleRequested, this, &MainWindow::onScreenShareToggleClicked);
    connect(chatView_, &ChatView::deleteMessageRequested, this,
            [this](qint64 id) { chatClient_.sendDeleteMessage(id); });
    connect(&callManager_, &CallManager::participantJoined, this, [this](const QString& login) {
        if (!callParticipants_.contains(login)) {
            callParticipants_.append(login);
        }
        chatView_->setCallParticipants(callParticipants_);
    });
    connect(&callManager_, &CallManager::participantLeft, this, [this](const QString& login) {
        callParticipants_.removeAll(login);
        chatView_->setCallParticipants(callParticipants_);
    });
    connect(&callManager_, &CallManager::callError, this,
            [this](const QString& message) { showToast(message, ToastBanner::Variant::kError); });
    connect(&callManager_, &CallManager::remoteVideoFrameReceived, this,
            [this](const QString& login, const QImage& frame) { chatView_->showRemoteVideoFrame(login, frame); });
    connect(&callManager_, &CallManager::remoteVideoTrackRemoved, this,
            [this](const QString& login) { chatView_->removeRemoteVideo(login); });

    connect(communitiesPanel_, &CommunitiesPanel::createRequested, this, [this](const QString& name) {
        if (lastToken_.isEmpty()) {
            showToast(tr("Sign in first (Account menu, top right)"), ToastBanner::Variant::kInfo);
            return;
        }
        chatRestClient_.createCommunity(lastToken_, name);
    });
    connect(communitiesPanel_, &CommunitiesPanel::renameRequested, this,
            [this](qint64 id, const QString& newName) { chatRestClient_.renameCommunity(lastToken_, id, newName); });
    connect(communitiesPanel_, &CommunitiesPanel::deleteRequested, this,
            [this](qint64 id) { chatRestClient_.deleteCommunity(lastToken_, id); });
    connect(communitiesPanel_, &CommunitiesPanel::joinRequested, this,
            [this](qint64 id) { chatRestClient_.joinCommunity(lastToken_, id); });
    connect(communitiesPanel_, &CommunitiesPanel::communitySelected, this, [this](qint64 id) {
        selectedCommunityId_ = id;
        closeChatView();
        refreshChannelsForSelectedCommunity();
    });

    connect(channelsPanel_, &ChannelsPanel::createRequested, this, [this](const QString& name) {
        if (selectedCommunityId_ < 0) {
            showToast(tr("Pick a community first"), ToastBanner::Variant::kInfo);
            return;
        }
        chatRestClient_.createChannel(lastToken_, selectedCommunityId_, name);
    });
    connect(channelsPanel_, &ChannelsPanel::renameRequested, this,
            [this](qint64 id, const QString& newName) { chatRestClient_.renameChannel(lastToken_, id, newName); });
    connect(channelsPanel_, &ChannelsPanel::deleteRequested, this,
            [this](qint64 id) { chatRestClient_.deleteChannel(lastToken_, id); });
    connect(channelsPanel_, &ChannelsPanel::channelSelected, this,
            [this](qint64 id, const QString& name) { openChannel(id, name); });

    connect(chatView_, &ChatView::createChannelRequested, channelsPanel_->addButton(), &QPushButton::click);

    connect(&chatRestClient_, &ChatRestClient::communityCreated, this, [this](qint64 id, const QString& name) {
        showToast(tr("Community '%1' created").arg(name), ToastBanner::Variant::kSuccess);
        pendingCommunitySelection_ = id;
        refreshCommunities();
    });
    connect(&chatRestClient_, &ChatRestClient::communitiesListed, this, [this](const QList<ChatItem>& communities) {
        communities_ = communities;
        communitiesPanel_->setCommunities(communities_);
        if (pendingCommunitySelection_ >= 0) {
            communitiesPanel_->selectCommunityId(pendingCommunitySelection_);
            selectedCommunityId_ = pendingCommunitySelection_;
            refreshChannelsForSelectedCommunity();
        }
        pendingCommunitySelection_ = -1;
    });
    connect(&chatRestClient_, &ChatRestClient::communityRenamed, this, [this](qint64, const QString& newName) {
        showToast(tr("Community renamed to '%1'").arg(newName), ToastBanner::Variant::kSuccess);
        refreshCommunities();
    });
    connect(&chatRestClient_, &ChatRestClient::communityDeleted, this, [this](qint64 id) {
        showToast(tr("Community deleted"), ToastBanner::Variant::kSuccess);
        if (id == selectedCommunityId_) {
            selectedCommunityId_ = -1;
            channelsPanel_->setChannels({});
            closeChatView();
        }
        refreshCommunities();
    });
    connect(&chatRestClient_, &ChatRestClient::communityJoined, this, [this](qint64) {
        showToast(tr("Joined community"), ToastBanner::Variant::kSuccess);
    });
    connect(&chatRestClient_, &ChatRestClient::channelCreated, this, [this](qint64 id, const QString& name) {
        showToast(tr("Channel '%1' created").arg(name), ToastBanner::Variant::kSuccess);
        pendingChannelSelection_ = id;
        refreshChannelsForSelectedCommunity();
    });
    connect(&chatRestClient_, &ChatRestClient::channelsListed, this, [this](const QList<ChatItem>& channels) {
        channels_ = channels;
        channelsPanel_->setChannels(channels_);
        if (pendingChannelSelection_ >= 0) {
            channelsPanel_->selectChannelId(pendingChannelSelection_);
            const auto it = std::find_if(channels_.cbegin(), channels_.cend(),
                                          [this](const ChatItem& item) { return item.id == pendingChannelSelection_; });
            if (it != channels_.cend()) {
                openChannel(it->id, it->name);
            }
        }
        pendingChannelSelection_ = -1;
    });
    connect(&chatRestClient_, &ChatRestClient::channelRenamed, this, [this](qint64 id, const QString& newName) {
        showToast(tr("Channel renamed to '%1'").arg(newName), ToastBanner::Variant::kSuccess);
        if (id == selectedChannelId_) {
            chatView_->showChannel(newName);
        }
        refreshChannelsForSelectedCommunity();
    });
    connect(&chatRestClient_, &ChatRestClient::channelDeleted, this, [this](qint64 id) {
        showToast(tr("Channel deleted"), ToastBanner::Variant::kSuccess);
        if (id == selectedChannelId_) {
            closeChatView();
        }
        refreshChannelsForSelectedCommunity();
    });
    connect(&chatRestClient_, &ChatRestClient::messagesListed, this,
            [this](qint64 channelId, const QList<ChatMessageInfo>& messages) {
                if (channelId != selectedChannelId_) {
                    return;  // Stale reply for a channel we've since left.
                }
                QList<ChatMessage> converted;
                converted.reserve(messages.size());
                for (const ChatMessageInfo& info : messages) {
                    converted.append(
                        ChatMessage{.id = info.id, .author = info.author, .body = info.body, .sentAt = info.sentAt});
                }
                if (oldestMessageId_ < 0) {
                    // Initial history load for this channel — list was
                    // empty, so appending in chronological order (as
                    // returned) reads the same as prepending would.
                    for (const ChatMessage& message : converted) {
                        chatView_->appendMessage(message);
                    }
                } else {
                    chatView_->prependMessages(converted);
                }
                if (!converted.isEmpty()) {
                    oldestMessageId_ = converted.first().id;
                }
                chatView_->setLoadOlderVisible(messages.size() == kMessagePageSize);
            });
    connect(chatView_, &ChatView::loadOlderMessagesRequested, this, [this]() {
        if (selectedChannelId_ >= 0) {
            chatRestClient_.listMessages(lastToken_, selectedChannelId_, kMessagePageSize, oldestMessageId_);
        }
    });
    connect(&chatRestClient_, &ChatRestClient::errorOccurred, this, [this](const QString& message) {
        showToast(tr("Error: %1").arg(message), ToastBanner::Variant::kError);
    });

    connect(footerBar_->settingsButton(), &QPushButton::clicked, this, [this]() {
        settingsDialog_->show();
        settingsDialog_->raise();
        settingsDialog_->activateWindow();
    });

    // CameraDevice owns the one video sink QMediaCaptureSession allows
    // (see its doc comment) — the preview widget gets frames pushed
    // manually instead of being attached as the session's output
    // directly, so the same frames are also available for a call's
    // outgoing video (issue #72).
    connect(&camera_, &CameraDevice::frameAvailable, this,
            [this](const QVideoFrame& frame) { settingsDialog_->videoPreview()->videoSink()->setVideoFrame(frame); });
    // Same fan-out, third consumer: the call's own local-preview tile
    // (issue #91). Frames only actually flow while the camera is
    // running (Settings preview or CallManager::enableVideo()), so this
    // is safe to leave connected unconditionally.
    connect(&camera_, &CameraDevice::frameAvailable, this,
            [this](const QVideoFrame& frame) { chatView_->localVideoWidget()->videoSink()->setVideoFrame(frame); });
    // Same "owns the one sink, re-emits" refactor as CameraDevice (issue
    // #112) — screen share and camera video share the one local-preview
    // tile/track, mutually exclusive (CallManager::enableScreenShare()
    // stops the camera and vice versa), so both fan out to it the same way.
    connect(&screenCapture_, &ScreenCaptureDevice::frameAvailable, this,
            [this](const QVideoFrame& frame) { settingsDialog_->screenPreview()->videoSink()->setVideoFrame(frame); });
    connect(&screenCapture_, &ScreenCaptureDevice::frameAvailable, this,
            [this](const QVideoFrame& frame) { chatView_->localVideoWidget()->videoSink()->setVideoFrame(frame); });
}

MainWindow::~MainWindow() = default;

void MainWindow::buildUi() {
    setWindowTitle(tr("DeviceHub"));
    resize(1000, 700);

    auto* central = new QWidget(this);
    auto* rootLayout = new QVBoxLayout(central);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    auto* topBar = new QWidget(central);
    topBar->setObjectName(QStringLiteral("topBar"));
    topBar->setAttribute(Qt::WA_StyledBackground, true);
    auto* topBarLayout = new QHBoxLayout(topBar);
    topBarLayout->setContentsMargins(ui_theme::kSpacingMd, ui_theme::kSpacingSm, ui_theme::kSpacingMd,
                                      ui_theme::kSpacingSm);
    accountMenu_ = new AccountMenu(topBar);
    topBarLayout->addStretch();
    topBarLayout->addWidget(accountMenu_);

    auto* sidebar = new QWidget(central);
    sidebar->setObjectName(QStringLiteral("sidebar"));
    sidebar->setAttribute(Qt::WA_StyledBackground, true);
    sidebar->setFixedWidth(280);
    auto* sidebarLayout = new QHBoxLayout(sidebar);
    sidebarLayout->setContentsMargins(0, 0, 0, 0);
    sidebarLayout->setSpacing(0);
    communitiesPanel_ = new CommunitiesPanel(sidebar);
    channelsPanel_ = new ChannelsPanel(sidebar);
    sidebarLayout->addWidget(communitiesPanel_);
    sidebarLayout->addWidget(channelsPanel_, /*stretch=*/1);

    chatView_ = new ChatView(central);
    toastBanner_ = new ToastBanner(chatView_);
    desktopNotifier_ = new DesktopNotifier(this, this);

    auto* middleLayout = new QHBoxLayout;
    middleLayout->setContentsMargins(0, 0, 0, 0);
    middleLayout->addWidget(sidebar);
    middleLayout->addWidget(chatView_, /*stretch=*/1);

    footerBar_ = new FooterBar(central);

    rootLayout->addWidget(topBar);
    rootLayout->addLayout(middleLayout, /*stretch=*/1);
    rootLayout->addWidget(footerBar_);

    setCentralWidget(central);

    settingsDialog_ = new SettingsDialog(this);
    profileDialog_ = new ProfileDialog(this);
}

void MainWindow::populateDevices() {
    for (const QAudioDevice& device : enumerator_.audioOutputs()) {
        settingsDialog_->outputCombo()->addItem(device.description(), QVariant::fromValue(device));
    }
    for (const QAudioDevice& device : enumerator_.audioInputs()) {
        settingsDialog_->inputCombo()->addItem(device.description(), QVariant::fromValue(device));
    }
    for (const QCameraDevice& device : enumerator_.cameras()) {
        settingsDialog_->cameraCombo()->addItem(device.description(), QVariant::fromValue(device));
    }

    screens_ = enumerator_.screens();
    for (const QScreen* screen : std::as_const(screens_)) {
        settingsDialog_->screenCombo()->addItem(screen->name());
    }
}

void MainWindow::onPlayToneClicked() {
    const QAudioDevice device = settingsDialog_->outputCombo()->currentData().value<QAudioDevice>();
    audioOutput_.playTestTone(device);
}

void MainWindow::onToggleMicClicked() {
    if (audioInput_.isCapturing()) {
        audioInput_.stop();
        settingsDialog_->toggleMicButton()->setText(tr("Start capture"));
        settingsDialog_->micLevelBar()->setValue(0);
    } else {
        settingsDialog_->micStatusLabel()->clear();
        const QAudioDevice device = settingsDialog_->inputCombo()->currentData().value<QAudioDevice>();
        audioInput_.start(device);
        settingsDialog_->toggleMicButton()->setText(tr("Stop capture"));
    }
}

void MainWindow::onToggleCameraClicked() {
    if (camera_.isActive()) {
        camera_.stop();
        settingsDialog_->toggleCameraButton()->setText(tr("Start camera"));
        return;
    }

    if (settingsDialog_->cameraCombo()->currentIndex() < 0) {
        settingsDialog_->cameraStatusLabel()->setText(tr("No camera available"));
        return;
    }

    settingsDialog_->cameraStatusLabel()->clear();
    const QCameraDevice device = settingsDialog_->cameraCombo()->currentData().value<QCameraDevice>();
    camera_.setDevice(device);
    camera_.start();
    settingsDialog_->toggleCameraButton()->setText(tr("Stop camera"));
}

void MainWindow::onToggleScreenCaptureClicked() {
    if (screenCapture_.isActive()) {
        screenCapture_.stop();
        settingsDialog_->toggleScreenCaptureButton()->setText(tr("Start screen capture"));
        return;
    }

    if (const int index = settingsDialog_->screenCombo()->currentIndex(); index >= 0 && index < screens_.size()) {
        settingsDialog_->screenStatusLabel()->clear();
        screenCapture_.setScreen(screens_[index]);
        screenCapture_.start();
        settingsDialog_->toggleScreenCaptureButton()->setText(tr("Stop screen capture"));
    } else {
        settingsDialog_->screenStatusLabel()->setText(tr("No screen available"));
    }
}

void MainWindow::onRequestTokenClicked() {
    accountMenu_->statusLabel()->setText(tr("Requesting token..."));
    authClient_.requestToken(accountMenu_->loginEdit()->text(), accountMenu_->passwordEdit()->text());
}

void MainWindow::onRegisterClicked() {
    accountMenu_->statusLabel()->setText(tr("Registering..."));
    authClient_.registerUser(accountMenu_->loginEdit()->text(), accountMenu_->passwordEdit()->text());
}

void MainWindow::onSendChatMessageClicked() {
    if (selectedChannelId_ < 0) {
        return;
    }
    const QString text = chatView_->messageEdit()->text();
    if (chatView_->editingMessageId() >= 0) {
        chatClient_.sendEditMessage(chatView_->editingMessageId(), text);
        chatView_->cancelEditingMessage();
    } else {
        chatClient_.sendMessage(text);
        chatView_->messageEdit()->clear();
    }
}

void MainWindow::onCallToggleClicked() {
    if (callManager_.inCall()) {
        if (callManager_.videoEnabled()) {
            callManager_.disableVideo();
        }
        if (callManager_.screenShareEnabled()) {
            callManager_.disableScreenShare();
        }
        callManager_.leaveCall();
        callParticipants_.clear();
        chatView_->setCallParticipants(callParticipants_);
    } else {
        const QAudioDevice inputDevice = settingsDialog_->inputCombo()->currentData().value<QAudioDevice>();
        const QAudioDevice outputDevice = settingsDialog_->outputCombo()->currentData().value<QAudioDevice>();
        callManager_.joinCall(inputDevice, outputDevice);
    }
    chatView_->setCallState(callManager_.inCall(), callManager_.isMuted());
}

void MainWindow::onMuteToggleClicked() {
    callManager_.setMuted(!callManager_.isMuted());
    chatView_->setCallState(callManager_.inCall(), callManager_.isMuted());
}

void MainWindow::onVideoToggleClicked() {
    if (callManager_.videoEnabled()) {
        callManager_.disableVideo();
    } else {
        const QCameraDevice cameraDevice = settingsDialog_->cameraCombo()->currentData().value<QCameraDevice>();
        callManager_.enableVideo(cameraDevice);
    }
    // enableVideo() may have just disabled screen share (CallManager's
    // camera video and screen share are mutually exclusive, issue #112)
    // — refresh both toggle buttons/the shared local preview together.
    chatView_->setVideoEnabled(callManager_.videoEnabled());
    chatView_->setScreenShareEnabled(callManager_.screenShareEnabled());
}

void MainWindow::onScreenShareToggleClicked() {
    if (callManager_.screenShareEnabled()) {
        callManager_.disableScreenShare();
    } else if (const int index = settingsDialog_->screenCombo()->currentIndex();
               index >= 0 && index < screens_.size()) {
        callManager_.enableScreenShare(screens_[index]);
    } else {
        showToast(tr("No screen available"), ToastBanner::Variant::kError);
    }
    chatView_->setVideoEnabled(callManager_.videoEnabled());
    chatView_->setScreenShareEnabled(callManager_.screenShareEnabled());
}

void MainWindow::onEditProfileClicked() {
    profileDialog_->statusLabel()->clear();
    profileDialog_->show();
    profileDialog_->raise();
    profileDialog_->activateWindow();
}

void MainWindow::refreshCommunities() {
    if (lastToken_.isEmpty()) {
        showToast(tr("Sign in first (Account menu, top right)"), ToastBanner::Variant::kInfo);
        return;
    }
    chatRestClient_.listCommunities(lastToken_);
}

void MainWindow::refreshChannelsForSelectedCommunity() {
    if (selectedCommunityId_ < 0) {
        channelsPanel_->setChannels({});
        return;
    }
    chatRestClient_.listChannels(lastToken_, selectedCommunityId_);
}

void MainWindow::openChannel(qint64 id, const QString& name) {
    // A call is scoped to whichever channel we're subscribed to — leave
    // it before switching, rather than stranding PeerConnections tied
    // to a channel we're no longer even connected to.
    if (callManager_.inCall()) {
        if (callManager_.videoEnabled()) {
            callManager_.disableVideo();
        }
        if (callManager_.screenShareEnabled()) {
            callManager_.disableScreenShare();
        }
        callManager_.leaveCall();
        callParticipants_.clear();
        chatView_->setCallParticipants(callParticipants_);
        chatView_->setCallState(false, callManager_.isMuted());
    }
    selectedChannelId_ = id;
    oldestMessageId_ = -1;
    chatClient_.disconnectFromChannel();
    chatView_->showChannel(name);
    chatView_->clearLog();
    chatClient_.connectToChannel(lastToken_, id);
    chatRestClient_.listMessages(lastToken_, id, kMessagePageSize);
}

void MainWindow::closeChatView() {
    if (callManager_.inCall()) {
        if (callManager_.videoEnabled()) {
            callManager_.disableVideo();
        }
        if (callManager_.screenShareEnabled()) {
            callManager_.disableScreenShare();
        }
        callManager_.leaveCall();
        callParticipants_.clear();
        chatView_->setCallParticipants(callParticipants_);
        chatView_->setCallState(false, callManager_.isMuted());
    }
    if (selectedChannelId_ >= 0) {
        chatClient_.disconnectFromChannel();
    }
    selectedChannelId_ = -1;
    oldestMessageId_ = -1;
    chatView_->showPlaceholder();
}

void MainWindow::showToast(const QString& text, ToastBanner::Variant variant) {
    toastBanner_->showMessage(text, variant, kToastTimeoutMs);
}

}  // namespace devicehub
