#include "ui/MainWindow.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QScreen>
#include <QVBoxLayout>
#include <QVideoWidget>
#include <QWidget>

#include <utility>

#include "ui/AccountMenu.h"
#include "ui/ChatPanel.h"
#include "ui/CommunitiesPanel.h"
#include "ui/FooterBar.h"
#include "ui/SettingsDialog.h"

namespace devicehub {

namespace {
constexpr const char* kDefaultAuthServiceUrl = "http://127.0.0.1:8080";
constexpr const char* kDefaultChatServiceWsUrl = "ws://127.0.0.1:8083";
constexpr const char* kDefaultChatServiceUrl = "http://127.0.0.1:8082";
}  // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent),
      authClient_(QUrl(qEnvironmentVariable("AUTH_SERVICE_URL", kDefaultAuthServiceUrl))),
      chatClient_(QUrl(qEnvironmentVariable("CHAT_SERVICE_WS_URL", kDefaultChatServiceWsUrl))),
      chatRestClient_(QUrl(qEnvironmentVariable("CHAT_SERVICE_URL", kDefaultChatServiceUrl))) {
    buildUi();
    populateDevices();

    connect(settingsDialog_->playToneButton(), &QPushButton::clicked, this, &MainWindow::onPlayToneClicked);
    connect(settingsDialog_->toggleMicButton(), &QPushButton::clicked, this, &MainWindow::onToggleMicClicked);
    connect(settingsDialog_->toggleCameraButton(), &QPushButton::clicked, this, &MainWindow::onToggleCameraClicked);
    connect(settingsDialog_->toggleScreenCaptureButton(), &QPushButton::clicked, this,
            &MainWindow::onToggleScreenCaptureClicked);
    connect(accountMenu_->requestTokenButton(), &QPushButton::clicked, this, &MainWindow::onRequestTokenClicked);
    connect(accountMenu_->registerButton(), &QPushButton::clicked, this, &MainWindow::onRegisterClicked);
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
    connect(&authClient_, &AuthClient::tokenReceived, this, [this](const QString& token) {
        lastToken_ = token;
        accountMenu_->statusLabel()->setText(tr("Token received, verifying..."));
        authClient_.verifyToken(token);
    });
    connect(&authClient_, &AuthClient::tokenVerified, this, [this](bool valid, const QString& subject) {
        accountMenu_->statusLabel()->setText(valid ? tr("Verified — subject: %1").arg(subject) : tr("Token rejected"));
        footerBar_->setProfileText(valid ? subject : tr("Not signed in"));
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

    connect(chatPanel_->connectButton(), &QPushButton::clicked, this, &MainWindow::onConnectToChannelClicked);
    connect(chatPanel_->sendButton(), &QPushButton::clicked, this, &MainWindow::onSendChatMessageClicked);
    connect(&chatClient_, &ChatClient::subscribed, this, [this](qint64 channelId) {
        chatPanel_->chatLog()->appendPlainText(tr("-- subscribed to channel %1 --").arg(channelId));
    });
    connect(&chatClient_, &ChatClient::messageReceived, this,
            [this](const QString& author, const QString& body, const QString& sentAt) {
                chatPanel_->chatLog()->appendPlainText(QStringLiteral("[%1] %2: %3").arg(sentAt, author, body));
            });
    connect(&chatClient_, &ChatClient::errorOccurred, this, [this](const QString& message) {
        chatPanel_->chatLog()->appendPlainText(tr("-- error: %1 --").arg(message));
    });

    connect(communitiesPanel_->createButton(), &QPushButton::clicked, this, &MainWindow::onCreateCommunityClicked);
    connect(communitiesPanel_->refreshButton(), &QPushButton::clicked, this, &MainWindow::onRefreshCommunitiesClicked);
    connect(communitiesPanel_->joinButton(), &QPushButton::clicked, this, &MainWindow::onJoinCommunityClicked);
    connect(chatPanel_->createChannelButton(), &QPushButton::clicked, this, &MainWindow::onCreateChannelClicked);
    connect(chatPanel_->refreshChannelsButton(), &QPushButton::clicked, this, &MainWindow::onRefreshChannelsClicked);

    connect(&chatRestClient_, &ChatRestClient::communityCreated, this, [this](qint64 id, const QString& name) {
        chatPanel_->chatLog()->appendPlainText(tr("-- community '%1' created --").arg(name));
        pendingCommunitySelection_ = id;
        onRefreshCommunitiesClicked();
    });
    connect(&chatRestClient_, &ChatRestClient::communitiesListed, this, [this](const QList<ChatItem>& communities) {
        communities_ = communities;
        communitiesPanel_->communityCombo()->clear();
        for (const ChatItem& community : communities_) {
            communitiesPanel_->communityCombo()->addItem(community.name, community.id);
        }
        if (const int index = communitiesPanel_->communityCombo()->findData(pendingCommunitySelection_); index >= 0) {
            communitiesPanel_->communityCombo()->setCurrentIndex(index);
        }
        pendingCommunitySelection_ = -1;
    });
    connect(&chatRestClient_, &ChatRestClient::communityJoined, this, [this](qint64 communityId) {
        chatPanel_->chatLog()->appendPlainText(tr("-- joined community %1 --").arg(communityId));
    });
    connect(&chatRestClient_, &ChatRestClient::channelCreated, this, [this](qint64 id, const QString& name) {
        chatPanel_->chatLog()->appendPlainText(tr("-- channel '%1' created --").arg(name));
        pendingChannelSelection_ = id;
        onRefreshChannelsClicked();
    });
    connect(&chatRestClient_, &ChatRestClient::channelsListed, this, [this](const QList<ChatItem>& channels) {
        channels_ = channels;
        chatPanel_->channelCombo()->clear();
        for (const ChatItem& channel : channels_) {
            chatPanel_->channelCombo()->addItem(channel.name, channel.id);
        }
        if (const int index = chatPanel_->channelCombo()->findData(pendingChannelSelection_); index >= 0) {
            chatPanel_->channelCombo()->setCurrentIndex(index);
        }
        pendingChannelSelection_ = -1;
    });
    connect(&chatRestClient_, &ChatRestClient::errorOccurred, this, [this](const QString& message) {
        chatPanel_->chatLog()->appendPlainText(tr("-- error: %1 --").arg(message));
    });

    connect(footerBar_->settingsButton(), &QPushButton::clicked, this, [this]() {
        settingsDialog_->show();
        settingsDialog_->raise();
        settingsDialog_->activateWindow();
    });

    camera_.captureSession().setVideoOutput(settingsDialog_->videoPreview());
    screenCapture_.captureSession().setVideoOutput(settingsDialog_->screenPreview());
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
    accountMenu_ = new AccountMenu(topBar);
    topBarLayout->addStretch();
    topBarLayout->addWidget(accountMenu_);

    auto* sidebar = new QWidget(central);
    sidebar->setObjectName(QStringLiteral("sidebar"));
    sidebar->setAttribute(Qt::WA_StyledBackground, true);
    sidebar->setFixedWidth(280);
    auto* sidebarLayout = new QVBoxLayout(sidebar);
    sidebarLayout->setContentsMargins(0, 0, 0, 0);
    communitiesPanel_ = new CommunitiesPanel(sidebar);
    chatPanel_ = new ChatPanel(sidebar);
    sidebarLayout->addWidget(communitiesPanel_, /*stretch=*/1);
    sidebarLayout->addWidget(chatPanel_, /*stretch=*/2);

    auto* mainContentPlaceholder = new QLabel(tr("Select a channel to start chatting"), central);
    mainContentPlaceholder->setObjectName(QStringLiteral("mainContentPlaceholder"));
    mainContentPlaceholder->setAlignment(Qt::AlignCenter);

    auto* middleLayout = new QHBoxLayout;
    middleLayout->setContentsMargins(0, 0, 0, 0);
    middleLayout->addWidget(sidebar);
    middleLayout->addWidget(mainContentPlaceholder, /*stretch=*/1);

    footerBar_ = new FooterBar(central);

    rootLayout->addWidget(topBar);
    rootLayout->addLayout(middleLayout, /*stretch=*/1);
    rootLayout->addWidget(footerBar_);

    setCentralWidget(central);

    settingsDialog_ = new SettingsDialog(this);
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

void MainWindow::onConnectToChannelClicked() {
    if (lastToken_.isEmpty()) {
        chatPanel_->chatLog()->appendPlainText(tr("-- get a token first (Account menu, top right) --"));
        return;
    }
    if (const int index = chatPanel_->channelCombo()->currentIndex(); index >= 0) {
        chatClient_.connectToChannel(lastToken_, chatPanel_->channelCombo()->currentData().toLongLong());
    } else {
        chatPanel_->chatLog()->appendPlainText(tr("-- refresh and pick a channel first --"));
    }
}

void MainWindow::onSendChatMessageClicked() {
    chatClient_.sendMessage(chatPanel_->messageEdit()->text());
    chatPanel_->messageEdit()->clear();
}

void MainWindow::onCreateCommunityClicked() {
    if (lastToken_.isEmpty() || communitiesPanel_->nameEdit()->text().isEmpty()) {
        return;
    }
    chatRestClient_.createCommunity(lastToken_, communitiesPanel_->nameEdit()->text());
    communitiesPanel_->nameEdit()->clear();
}

void MainWindow::onRefreshCommunitiesClicked() {
    if (lastToken_.isEmpty()) {
        chatPanel_->chatLog()->appendPlainText(tr("-- get a token first (Account menu, top right) --"));
        return;
    }
    chatRestClient_.listCommunities(lastToken_);
}

void MainWindow::onJoinCommunityClicked() {
    if (lastToken_.isEmpty() || communitiesPanel_->communityCombo()->currentIndex() < 0) {
        return;
    }
    chatRestClient_.joinCommunity(lastToken_, communitiesPanel_->communityCombo()->currentData().toLongLong());
}

void MainWindow::onCreateChannelClicked() {
    if (lastToken_.isEmpty() || communitiesPanel_->communityCombo()->currentIndex() < 0 ||
        chatPanel_->channelNameEdit()->text().isEmpty()) {
        return;
    }
    chatRestClient_.createChannel(lastToken_, communitiesPanel_->communityCombo()->currentData().toLongLong(),
                                   chatPanel_->channelNameEdit()->text());
    chatPanel_->channelNameEdit()->clear();
}

void MainWindow::onRefreshChannelsClicked() {
    if (lastToken_.isEmpty() || communitiesPanel_->communityCombo()->currentIndex() < 0) {
        chatPanel_->chatLog()->appendPlainText(tr("-- pick a community first --"));
        return;
    }
    chatRestClient_.listChannels(lastToken_, communitiesPanel_->communityCombo()->currentData().toLongLong());
}

}  // namespace devicehub
