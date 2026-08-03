#include "ui/MainWindow.h"

#include <QComboBox>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QScreen>
#include <QVBoxLayout>
#include <QVideoWidget>

#include <utility>

namespace devicehub {

namespace {
constexpr const char* kDefaultAuthServiceUrl = "http://127.0.0.1:8080";
constexpr const char* kDefaultChatServiceWsUrl = "ws://127.0.0.1:8083";
}  // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent),
      authClient_(QUrl(qEnvironmentVariable("AUTH_SERVICE_URL", kDefaultAuthServiceUrl))),
      chatClient_(QUrl(qEnvironmentVariable("CHAT_SERVICE_WS_URL", kDefaultChatServiceWsUrl))) {
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

    camera_.captureSession().setVideoOutput(videoPreview_);
    screenCapture_.captureSession().setVideoOutput(screenPreview_);
}

MainWindow::~MainWindow() = default;

void MainWindow::buildUi() {
    setWindowTitle(tr("DeviceHub"));

    auto* central = new QWidget(this);
    auto* layout = new QVBoxLayout(central);

    auto* outputGroup = new QGroupBox(tr("Audio output"), central);
    auto* outputLayout = new QVBoxLayout(outputGroup);
    outputCombo_ = new QComboBox(outputGroup);
    outputCombo_->setObjectName(QStringLiteral("outputCombo"));
    playToneButton_ = new QPushButton(tr("Play test tone"), outputGroup);
    playToneButton_->setObjectName(QStringLiteral("playToneButton"));
    outputLayout->addWidget(outputCombo_);
    outputLayout->addWidget(playToneButton_);

    auto* inputGroup = new QGroupBox(tr("Microphone"), central);
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

    auto* cameraGroup = new QGroupBox(tr("Camera"), central);
    auto* cameraLayout = new QVBoxLayout(cameraGroup);
    cameraCombo_ = new QComboBox(cameraGroup);
    cameraCombo_->setObjectName(QStringLiteral("cameraCombo"));
    toggleCameraButton_ = new QPushButton(tr("Start camera"), cameraGroup);
    toggleCameraButton_->setObjectName(QStringLiteral("toggleCameraButton"));
    videoPreview_ = new QVideoWidget(cameraGroup);
    videoPreview_->setMinimumSize(320, 240);
    cameraLayout->addWidget(cameraCombo_);
    cameraLayout->addWidget(toggleCameraButton_);
    cameraLayout->addWidget(videoPreview_);

    auto* screenGroup = new QGroupBox(tr("Screen capture"), central);
    auto* screenLayout = new QVBoxLayout(screenGroup);
    screenCombo_ = new QComboBox(screenGroup);
    screenCombo_->setObjectName(QStringLiteral("screenCombo"));
    toggleScreenCaptureButton_ = new QPushButton(tr("Start screen capture"), screenGroup);
    toggleScreenCaptureButton_->setObjectName(QStringLiteral("toggleScreenCaptureButton"));
    screenPreview_ = new QVideoWidget(screenGroup);
    screenPreview_->setMinimumSize(320, 240);
    screenLayout->addWidget(screenCombo_);
    screenLayout->addWidget(toggleScreenCaptureButton_);
    screenLayout->addWidget(screenPreview_);

    auto* authGroup = new QGroupBox(tr("Authorization"), central);
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

    auto* chatGroup = new QGroupBox(tr("Chat"), central);
    auto* chatLayout = new QVBoxLayout(chatGroup);
    channelIdEdit_ = new QLineEdit(chatGroup);
    channelIdEdit_->setObjectName(QStringLiteral("channelIdEdit"));
    channelIdEdit_->setPlaceholderText(tr("Channel ID"));
    connectToChannelButton_ = new QPushButton(tr("Connect to channel"), chatGroup);
    connectToChannelButton_->setObjectName(QStringLiteral("connectToChannelButton"));
    chatLog_ = new QPlainTextEdit(chatGroup);
    chatLog_->setObjectName(QStringLiteral("chatLog"));
    chatLog_->setReadOnly(true);
    chatMessageEdit_ = new QLineEdit(chatGroup);
    chatMessageEdit_->setObjectName(QStringLiteral("chatMessageEdit"));
    chatMessageEdit_->setPlaceholderText(tr("Message"));
    sendChatMessageButton_ = new QPushButton(tr("Send"), chatGroup);
    sendChatMessageButton_->setObjectName(QStringLiteral("sendChatMessageButton"));
    chatLayout->addWidget(channelIdEdit_);
    chatLayout->addWidget(connectToChannelButton_);
    chatLayout->addWidget(chatLog_);
    chatLayout->addWidget(chatMessageEdit_);
    chatLayout->addWidget(sendChatMessageButton_);

    layout->addWidget(outputGroup);
    layout->addWidget(inputGroup);
    layout->addWidget(cameraGroup);
    layout->addWidget(screenGroup);
    layout->addWidget(authGroup);
    layout->addWidget(chatGroup);

    setCentralWidget(central);
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
    } else {
        const QCameraDevice device = cameraCombo_->currentData().value<QCameraDevice>();
        camera_.setDevice(device);
        camera_.start();
        toggleCameraButton_->setText(tr("Stop camera"));
    }
}

void MainWindow::onToggleScreenCaptureClicked() {
    if (screenCapture_.isActive()) {
        screenCapture_.stop();
        toggleScreenCaptureButton_->setText(tr("Start screen capture"));
        return;
    }

    if (const int index = screenCombo_->currentIndex(); index >= 0 && index < screens_.size()) {
        screenCapture_.setScreen(screens_[index]);
        screenCapture_.start();
        toggleScreenCaptureButton_->setText(tr("Stop screen capture"));
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
    chatClient_.connectToChannel(lastToken_, channelIdEdit_->text().toLongLong());
}

void MainWindow::onSendChatMessageClicked() {
    chatClient_.sendMessage(chatMessageEdit_->text());
    chatMessageEdit_->clear();
}

}  // namespace devicehub
