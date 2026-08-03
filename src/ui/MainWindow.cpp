#include "ui/MainWindow.h"

#include <QComboBox>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QScreen>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QVideoWidget>

#include <utility>

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

    connect(playToneButton_, &QPushButton::clicked, this, &MainWindow::onPlayToneClicked);
    connect(toggleMicButton_, &QPushButton::clicked, this, &MainWindow::onToggleMicClicked);
    connect(toggleCameraButton_, &QPushButton::clicked, this, &MainWindow::onToggleCameraClicked);
    connect(toggleScreenCaptureButton_, &QPushButton::clicked, this, &MainWindow::onToggleScreenCaptureClicked);
    connect(requestTokenButton_, &QPushButton::clicked, this, &MainWindow::onRequestTokenClicked);
    connect(&audioInput_, &AudioInputDevice::levelChanged, micLevelBar_, [this](float level) {
        micLevelBar_->setValue(static_cast<int>(level * 100.0f));
    });
    connect(&camera_, &CameraDevice::errorOccurred, this,
            [this](const QString& message) { cameraStatusLabel_->setText(tr("Error: %1").arg(message)); });
    connect(&screenCapture_, &ScreenCaptureDevice::errorOccurred, this,
            [this](const QString& message) { screenStatusLabel_->setText(tr("Error: %1").arg(message)); });
    connect(&authClient_, &AuthClient::tokenReceived, this, [this](const QString& token) {
        lastToken_ = token;
        authStatusLabel_->setText(tr("Token received, verifying..."));
        authClient_.verifyToken(token);
    });
    connect(&authClient_, &AuthClient::tokenVerified, this, [this](bool valid, const QString& subject) {
        authStatusLabel_->setText(valid ? tr("Verified — subject: %1").arg(subject) : tr("Token rejected"));
    });
    connect(&authClient_, &AuthClient::errorOccurred, this, [this](const QString& message) {
        authStatusLabel_->setText(tr("Error: %1").arg(message));
    });

    connect(connectToChannelButton_, &QPushButton::clicked, this, &MainWindow::onConnectToChannelClicked);
    connect(sendChatMessageButton_, &QPushButton::clicked, this, &MainWindow::onSendChatMessageClicked);
    connect(&chatClient_, &ChatClient::subscribed, this, [this](qint64 channelId) {
        chatLog_->appendPlainText(tr("-- subscribed to channel %1 --").arg(channelId));
    });
    connect(&chatClient_, &ChatClient::messageReceived, this,
            [this](const QString& author, const QString& body, const QString& sentAt) {
                chatLog_->appendPlainText(QStringLiteral("[%1] %2: %3").arg(sentAt, author, body));
            });
    connect(&chatClient_, &ChatClient::errorOccurred, this, [this](const QString& message) {
        chatLog_->appendPlainText(tr("-- error: %1 --").arg(message));
    });

    connect(createCommunityButton_, &QPushButton::clicked, this, &MainWindow::onCreateCommunityClicked);
    connect(refreshCommunitiesButton_, &QPushButton::clicked, this, &MainWindow::onRefreshCommunitiesClicked);
    connect(joinCommunityButton_, &QPushButton::clicked, this, &MainWindow::onJoinCommunityClicked);
    connect(createChannelButton_, &QPushButton::clicked, this, &MainWindow::onCreateChannelClicked);
    connect(refreshChannelsButton_, &QPushButton::clicked, this, &MainWindow::onRefreshChannelsClicked);

    connect(&chatRestClient_, &ChatRestClient::communityCreated, this, [this](qint64, const QString& name) {
        chatLog_->appendPlainText(tr("-- community '%1' created --").arg(name));
        onRefreshCommunitiesClicked();
    });
    connect(&chatRestClient_, &ChatRestClient::communitiesListed, this, [this](const QList<ChatItem>& communities) {
        communities_ = communities;
        communityCombo_->clear();
        for (const ChatItem& community : communities_) {
            communityCombo_->addItem(community.name, community.id);
        }
    });
    connect(&chatRestClient_, &ChatRestClient::communityJoined, this,
            [this](qint64 communityId) { chatLog_->appendPlainText(tr("-- joined community %1 --").arg(communityId)); });
    connect(&chatRestClient_, &ChatRestClient::channelCreated, this, [this](qint64, const QString& name) {
        chatLog_->appendPlainText(tr("-- channel '%1' created --").arg(name));
        onRefreshChannelsClicked();
    });
    connect(&chatRestClient_, &ChatRestClient::channelsListed, this, [this](const QList<ChatItem>& channels) {
        channels_ = channels;
        channelCombo_->clear();
        for (const ChatItem& channel : channels_) {
            channelCombo_->addItem(channel.name, channel.id);
        }
    });
    connect(&chatRestClient_, &ChatRestClient::errorOccurred, this, [this](const QString& message) {
        chatLog_->appendPlainText(tr("-- error: %1 --").arg(message));
    });

    camera_.captureSession().setVideoOutput(videoPreview_);
    screenCapture_.captureSession().setVideoOutput(screenPreview_);
}

MainWindow::~MainWindow() = default;

void MainWindow::buildUi() {
    setWindowTitle(tr("DeviceHub"));

    auto* tabs = new QTabWidget(this);
    tabs->setObjectName(QStringLiteral("mainTabs"));

    auto* outputGroup = new QGroupBox(tr("Audio output"));
    auto* outputLayout = new QVBoxLayout(outputGroup);
    outputCombo_ = new QComboBox(outputGroup);
    outputCombo_->setObjectName(QStringLiteral("outputCombo"));
    playToneButton_ = new QPushButton(tr("Play test tone"), outputGroup);
    playToneButton_->setObjectName(QStringLiteral("playToneButton"));
    outputLayout->addWidget(outputCombo_);
    outputLayout->addWidget(playToneButton_);

    auto* inputGroup = new QGroupBox(tr("Microphone"));
    auto* inputLayout = new QVBoxLayout(inputGroup);
    inputCombo_ = new QComboBox(inputGroup);
    inputCombo_->setObjectName(QStringLiteral("inputCombo"));
    toggleMicButton_ = new QPushButton(tr("Start capture"), inputGroup);
    toggleMicButton_->setObjectName(QStringLiteral("toggleMicButton"));
    micLevelBar_ = new QProgressBar(inputGroup);
    micLevelBar_->setRange(0, 100);
    inputLayout->addWidget(inputCombo_);
    inputLayout->addWidget(toggleMicButton_);
    inputLayout->addWidget(micLevelBar_);

    auto* cameraGroup = new QGroupBox(tr("Camera"));
    auto* cameraLayout = new QVBoxLayout(cameraGroup);
    cameraCombo_ = new QComboBox(cameraGroup);
    cameraCombo_->setObjectName(QStringLiteral("cameraCombo"));
    toggleCameraButton_ = new QPushButton(tr("Start camera"), cameraGroup);
    toggleCameraButton_->setObjectName(QStringLiteral("toggleCameraButton"));
    videoPreview_ = new QVideoWidget(cameraGroup);
    videoPreview_->setMinimumSize(320, 240);
    cameraStatusLabel_ = new QLabel(cameraGroup);
    cameraStatusLabel_->setObjectName(QStringLiteral("cameraStatusLabel"));
    cameraLayout->addWidget(cameraCombo_);
    cameraLayout->addWidget(toggleCameraButton_);
    cameraLayout->addWidget(videoPreview_);
    cameraLayout->addWidget(cameraStatusLabel_);

    auto* screenGroup = new QGroupBox(tr("Screen capture"));
    auto* screenLayout = new QVBoxLayout(screenGroup);
    screenCombo_ = new QComboBox(screenGroup);
    screenCombo_->setObjectName(QStringLiteral("screenCombo"));
    toggleScreenCaptureButton_ = new QPushButton(tr("Start screen capture"), screenGroup);
    toggleScreenCaptureButton_->setObjectName(QStringLiteral("toggleScreenCaptureButton"));
    screenPreview_ = new QVideoWidget(screenGroup);
    screenPreview_->setMinimumSize(320, 240);
    screenStatusLabel_ = new QLabel(screenGroup);
    screenStatusLabel_->setObjectName(QStringLiteral("screenStatusLabel"));
    screenLayout->addWidget(screenCombo_);
    screenLayout->addWidget(toggleScreenCaptureButton_);
    screenLayout->addWidget(screenPreview_);
    screenLayout->addWidget(screenStatusLabel_);

    auto* authGroup = new QGroupBox(tr("Authorization"));
    auto* authLayout = new QVBoxLayout(authGroup);
    loginEdit_ = new QLineEdit(authGroup);
    loginEdit_->setObjectName(QStringLiteral("loginEdit"));
    loginEdit_->setPlaceholderText(tr("Login"));
    passwordEdit_ = new QLineEdit(authGroup);
    passwordEdit_->setObjectName(QStringLiteral("passwordEdit"));
    passwordEdit_->setPlaceholderText(tr("Password"));
    passwordEdit_->setEchoMode(QLineEdit::Password);
    requestTokenButton_ = new QPushButton(tr("Get token & verify"), authGroup);
    requestTokenButton_->setObjectName(QStringLiteral("requestTokenButton"));
    authStatusLabel_ = new QLabel(tr("No token requested yet"), authGroup);
    authStatusLabel_->setObjectName(QStringLiteral("authStatusLabel"));
    authLayout->addWidget(loginEdit_);
    authLayout->addWidget(passwordEdit_);
    authLayout->addWidget(requestTokenButton_);
    authLayout->addWidget(authStatusLabel_);

    auto* chatGroup = new QGroupBox(tr("Chat"));
    auto* chatLayout = new QVBoxLayout(chatGroup);

    communityNameEdit_ = new QLineEdit(chatGroup);
    communityNameEdit_->setObjectName(QStringLiteral("communityNameEdit"));
    communityNameEdit_->setPlaceholderText(tr("New community name"));
    createCommunityButton_ = new QPushButton(tr("Create community"), chatGroup);
    createCommunityButton_->setObjectName(QStringLiteral("createCommunityButton"));
    communityCombo_ = new QComboBox(chatGroup);
    communityCombo_->setObjectName(QStringLiteral("communityCombo"));
    refreshCommunitiesButton_ = new QPushButton(tr("Refresh communities"), chatGroup);
    refreshCommunitiesButton_->setObjectName(QStringLiteral("refreshCommunitiesButton"));
    joinCommunityButton_ = new QPushButton(tr("Join selected community"), chatGroup);
    joinCommunityButton_->setObjectName(QStringLiteral("joinCommunityButton"));

    channelNameEdit_ = new QLineEdit(chatGroup);
    channelNameEdit_->setObjectName(QStringLiteral("channelNameEdit"));
    channelNameEdit_->setPlaceholderText(tr("New channel name"));
    createChannelButton_ = new QPushButton(tr("Create channel in selected community"), chatGroup);
    createChannelButton_->setObjectName(QStringLiteral("createChannelButton"));
    channelCombo_ = new QComboBox(chatGroup);
    channelCombo_->setObjectName(QStringLiteral("channelCombo"));
    refreshChannelsButton_ = new QPushButton(tr("Refresh channels"), chatGroup);
    refreshChannelsButton_->setObjectName(QStringLiteral("refreshChannelsButton"));
    connectToChannelButton_ = new QPushButton(tr("Connect to selected channel"), chatGroup);
    connectToChannelButton_->setObjectName(QStringLiteral("connectToChannelButton"));

    chatLog_ = new QPlainTextEdit(chatGroup);
    chatLog_->setObjectName(QStringLiteral("chatLog"));
    chatLog_->setReadOnly(true);
    chatMessageEdit_ = new QLineEdit(chatGroup);
    chatMessageEdit_->setObjectName(QStringLiteral("chatMessageEdit"));
    chatMessageEdit_->setPlaceholderText(tr("Message"));
    sendChatMessageButton_ = new QPushButton(tr("Send"), chatGroup);
    sendChatMessageButton_->setObjectName(QStringLiteral("sendChatMessageButton"));

    chatLayout->addWidget(communityNameEdit_);
    chatLayout->addWidget(createCommunityButton_);
    chatLayout->addWidget(communityCombo_);
    chatLayout->addWidget(refreshCommunitiesButton_);
    chatLayout->addWidget(joinCommunityButton_);
    chatLayout->addWidget(channelNameEdit_);
    chatLayout->addWidget(createChannelButton_);
    chatLayout->addWidget(channelCombo_);
    chatLayout->addWidget(refreshChannelsButton_);
    chatLayout->addWidget(connectToChannelButton_);
    chatLayout->addWidget(chatLog_);
    chatLayout->addWidget(chatMessageEdit_);
    chatLayout->addWidget(sendChatMessageButton_);

    tabs->addTab(outputGroup, tr("Audio Output"));
    tabs->addTab(inputGroup, tr("Microphone"));
    tabs->addTab(cameraGroup, tr("Camera"));
    tabs->addTab(screenGroup, tr("Screen Capture"));
    tabs->addTab(authGroup, tr("Authorization"));
    tabs->addTab(chatGroup, tr("Chat"));

    setCentralWidget(tabs);
}

void MainWindow::populateDevices() {
    for (const QAudioDevice& device : enumerator_.audioOutputs()) {
        outputCombo_->addItem(device.description(), QVariant::fromValue(device));
    }
    for (const QAudioDevice& device : enumerator_.audioInputs()) {
        inputCombo_->addItem(device.description(), QVariant::fromValue(device));
    }
    for (const QCameraDevice& device : enumerator_.cameras()) {
        cameraCombo_->addItem(device.description(), QVariant::fromValue(device));
    }

    screens_ = enumerator_.screens();
    for (const QScreen* screen : std::as_const(screens_)) {
        screenCombo_->addItem(screen->name());
    }
}

void MainWindow::onPlayToneClicked() {
    const QAudioDevice device = outputCombo_->currentData().value<QAudioDevice>();
    audioOutput_.playTestTone(device);
}

void MainWindow::onToggleMicClicked() {
    if (audioInput_.isCapturing()) {
        audioInput_.stop();
        toggleMicButton_->setText(tr("Start capture"));
        micLevelBar_->setValue(0);
    } else {
        const QAudioDevice device = inputCombo_->currentData().value<QAudioDevice>();
        audioInput_.start(device);
        toggleMicButton_->setText(tr("Stop capture"));
    }
}

void MainWindow::onToggleCameraClicked() {
    if (camera_.isActive()) {
        camera_.stop();
        toggleCameraButton_->setText(tr("Start camera"));
        return;
    }

    if (cameraCombo_->currentIndex() < 0) {
        cameraStatusLabel_->setText(tr("No camera available"));
        return;
    }

    cameraStatusLabel_->clear();
    const QCameraDevice device = cameraCombo_->currentData().value<QCameraDevice>();
    camera_.setDevice(device);
    camera_.start();
    toggleCameraButton_->setText(tr("Stop camera"));
}

void MainWindow::onToggleScreenCaptureClicked() {
    if (screenCapture_.isActive()) {
        screenCapture_.stop();
        toggleScreenCaptureButton_->setText(tr("Start screen capture"));
        return;
    }

    if (const int index = screenCombo_->currentIndex(); index >= 0 && index < screens_.size()) {
        screenStatusLabel_->clear();
        screenCapture_.setScreen(screens_[index]);
        screenCapture_.start();
        toggleScreenCaptureButton_->setText(tr("Stop screen capture"));
    } else {
        screenStatusLabel_->setText(tr("No screen available"));
    }
}

void MainWindow::onRequestTokenClicked() {
    authStatusLabel_->setText(tr("Requesting token..."));
    authClient_.requestToken(loginEdit_->text(), passwordEdit_->text());
}

void MainWindow::onConnectToChannelClicked() {
    if (lastToken_.isEmpty()) {
        chatLog_->appendPlainText(tr("-- get a token first (Authorization section) --"));
        return;
    }
    if (const int index = channelCombo_->currentIndex(); index >= 0) {
        chatClient_.connectToChannel(lastToken_, channelCombo_->currentData().toLongLong());
    } else {
        chatLog_->appendPlainText(tr("-- refresh and pick a channel first --"));
    }
}

void MainWindow::onSendChatMessageClicked() {
    chatClient_.sendMessage(chatMessageEdit_->text());
    chatMessageEdit_->clear();
}

void MainWindow::onCreateCommunityClicked() {
    if (lastToken_.isEmpty() || communityNameEdit_->text().isEmpty()) {
        return;
    }
    chatRestClient_.createCommunity(lastToken_, communityNameEdit_->text());
    communityNameEdit_->clear();
}

void MainWindow::onRefreshCommunitiesClicked() {
    if (lastToken_.isEmpty()) {
        chatLog_->appendPlainText(tr("-- get a token first (Authorization section) --"));
        return;
    }
    chatRestClient_.listCommunities(lastToken_);
}

void MainWindow::onJoinCommunityClicked() {
    if (lastToken_.isEmpty() || communityCombo_->currentIndex() < 0) {
        return;
    }
    chatRestClient_.joinCommunity(lastToken_, communityCombo_->currentData().toLongLong());
}

void MainWindow::onCreateChannelClicked() {
    if (lastToken_.isEmpty() || communityCombo_->currentIndex() < 0 || channelNameEdit_->text().isEmpty()) {
        return;
    }
    chatRestClient_.createChannel(lastToken_, communityCombo_->currentData().toLongLong(), channelNameEdit_->text());
    channelNameEdit_->clear();
}

void MainWindow::onRefreshChannelsClicked() {
    if (lastToken_.isEmpty() || communityCombo_->currentIndex() < 0) {
        chatLog_->appendPlainText(tr("-- pick a community first --"));
        return;
    }
    chatRestClient_.listChannels(lastToken_, communityCombo_->currentData().toLongLong());
}

}  // namespace devicehub
