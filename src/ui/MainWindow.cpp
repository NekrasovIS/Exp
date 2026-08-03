#include "ui/MainWindow.h"

#include <QComboBox>
#include <QGroupBox>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>
#include <QVideoWidget>

namespace devicehub {

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    buildUi();
    populateDevices();

    connect(playToneButton_, &QPushButton::clicked, this, &MainWindow::onPlayToneClicked);
    connect(toggleMicButton_, &QPushButton::clicked, this, &MainWindow::onToggleMicClicked);
    connect(toggleCameraButton_, &QPushButton::clicked, this, &MainWindow::onToggleCameraClicked);
    connect(&audioInput_, &AudioInputDevice::levelChanged, micLevelBar_, [this](float level) {
        micLevelBar_->setValue(static_cast<int>(level * 100.0f));
    });

    camera_.captureSession().setVideoOutput(videoPreview_);
}

MainWindow::~MainWindow() = default;

void MainWindow::buildUi() {
    setWindowTitle(tr("DeviceHub"));

    auto* central = new QWidget(this);
    auto* layout = new QVBoxLayout(central);

    auto* outputGroup = new QGroupBox(tr("Audio output"), central);
    auto* outputLayout = new QVBoxLayout(outputGroup);
    outputCombo_ = new QComboBox(outputGroup);
    playToneButton_ = new QPushButton(tr("Play test tone"), outputGroup);
    outputLayout->addWidget(outputCombo_);
    outputLayout->addWidget(playToneButton_);

    auto* inputGroup = new QGroupBox(tr("Microphone"), central);
    auto* inputLayout = new QVBoxLayout(inputGroup);
    inputCombo_ = new QComboBox(inputGroup);
    toggleMicButton_ = new QPushButton(tr("Start capture"), inputGroup);
    micLevelBar_ = new QProgressBar(inputGroup);
    micLevelBar_->setRange(0, 100);
    inputLayout->addWidget(inputCombo_);
    inputLayout->addWidget(toggleMicButton_);
    inputLayout->addWidget(micLevelBar_);

    auto* cameraGroup = new QGroupBox(tr("Camera"), central);
    auto* cameraLayout = new QVBoxLayout(cameraGroup);
    cameraCombo_ = new QComboBox(cameraGroup);
    toggleCameraButton_ = new QPushButton(tr("Start camera"), cameraGroup);
    videoPreview_ = new QVideoWidget(cameraGroup);
    videoPreview_->setMinimumSize(320, 240);
    cameraLayout->addWidget(cameraCombo_);
    cameraLayout->addWidget(toggleCameraButton_);
    cameraLayout->addWidget(videoPreview_);

    layout->addWidget(outputGroup);
    layout->addWidget(inputGroup);
    layout->addWidget(cameraGroup);

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

}  // namespace devicehub
