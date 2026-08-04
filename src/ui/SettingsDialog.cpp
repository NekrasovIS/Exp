#include "ui/SettingsDialog.h"

#include <QComboBox>
#include <QGroupBox>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QVideoWidget>

namespace devicehub {

SettingsDialog::SettingsDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Settings"));

    auto* tabs = new QTabWidget(this);
    tabs->setObjectName(QStringLiteral("settingsTabs"));

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
    micStatusLabel_ = new QLabel(inputGroup);
    micStatusLabel_->setObjectName(QStringLiteral("micStatusLabel"));
    inputLayout->addWidget(inputCombo_);
    inputLayout->addWidget(toggleMicButton_);
    inputLayout->addWidget(micLevelBar_);
    inputLayout->addWidget(micStatusLabel_);

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

    tabs->addTab(outputGroup, tr("Audio Output"));
    tabs->addTab(inputGroup, tr("Microphone"));
    tabs->addTab(cameraGroup, tr("Camera"));
    tabs->addTab(screenGroup, tr("Screen Capture"));

    auto* dialogLayout = new QVBoxLayout(this);
    dialogLayout->addWidget(tabs);
}

}  // namespace devicehub
