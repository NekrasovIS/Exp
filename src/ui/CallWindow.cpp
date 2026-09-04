#include "ui/CallWindow.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QSizePolicy>
#include <QVBoxLayout>
#include <QVideoWidget>

#include <utility>

#include "ui/Theme.h"

namespace devicehub {

namespace {
constexpr int kVideoTileSize = 160;

/// Ключ remoteVideoTiles_ для (@p peerLogin, @p isScreenShare) — камера
/// и демонстрация экрана одного участника (issue #185) получают разные
/// плитки, а не делят одну.
QString remoteTileKey(const QString& peerLogin, bool isScreenShare) {
    return peerLogin + (isScreenShare ? QStringLiteral("#screen") : QStringLiteral("#camera"));
}
}  // namespace

CallWindow::CallWindow(QWidget* parent) : QWidget(parent) {
    // Реальное отдельное окно (issue #185), а не встроенная в ChatView
    // область — иметь родителя всё равно удобно: MainWindow владеет
    // временем жизни через дерево QObject, не занимаясь ручным delete.
    setWindowFlag(Qt::Window, true);
    setWindowTitle(tr("Call"));
    resize(640, 480);

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(ui_theme::kSpacingMd, ui_theme::kSpacingMd, ui_theme::kSpacingMd,
                                    ui_theme::kSpacingMd);
    rootLayout->setSpacing(ui_theme::kSpacingSm);

    muteToggleButton_ = new QPushButton(tr("Mute"), this);
    muteToggleButton_->setObjectName(QStringLiteral("muteToggleButton"));
    connect(muteToggleButton_, &QPushButton::clicked, this, &CallWindow::muteToggleRequested);

    videoToggleButton_ = new QPushButton(tr("Enable Video"), this);
    videoToggleButton_->setObjectName(QStringLiteral("videoToggleButton"));
    connect(videoToggleButton_, &QPushButton::clicked, this, &CallWindow::videoToggleRequested);

    screenShareToggleButton_ = new QPushButton(tr("Share Screen"), this);
    screenShareToggleButton_->setObjectName(QStringLiteral("screenShareToggleButton"));
    connect(screenShareToggleButton_, &QPushButton::clicked, this, &CallWindow::screenShareToggleRequested);

    leaveCallButton_ = new QPushButton(tr("Leave call"), this);
    leaveCallButton_->setObjectName(QStringLiteral("leaveCallButton"));
    leaveCallButton_->setProperty("accent", true);
    connect(leaveCallButton_, &QPushButton::clicked, this, &CallWindow::leaveCallRequested);

    auto* controlsRow = new QHBoxLayout;
    controlsRow->setSpacing(ui_theme::kSpacingSm);
    controlsRow->addWidget(muteToggleButton_);
    controlsRow->addWidget(videoToggleButton_);
    controlsRow->addWidget(screenShareToggleButton_);
    controlsRow->addStretch(1);
    controlsRow->addWidget(leaveCallButton_);

    callParticipantsLabel_ = new QLabel(this);
    callParticipantsLabel_->setObjectName(QStringLiteral("mutedDescription"));
    callParticipantsLabel_->setWordWrap(true);
    callParticipantsLabel_->setVisible(false);

    // Локальное превью + по одной плитке на каждого удалённого
    // участника, сейчас отправляющего видео (issue #91, перенесено из
    // ChatView в issue #185) — скрывается, когда показывать нечего.
    videoStrip_ = new QWidget(this);
    videoStripLayout_ = new QHBoxLayout(videoStrip_);
    videoStripLayout_->setContentsMargins(0, 0, 0, 0);
    videoStripLayout_->setSpacing(ui_theme::kSpacingSm);
    videoStripLayout_->addStretch(1);

    localVideoWidget_ = new QVideoWidget(videoStrip_);
    localVideoWidget_->setObjectName(QStringLiteral("localVideoWidget"));
    localVideoWidget_->setMinimumSize(kVideoTileSize, kVideoTileSize);
    localVideoWidget_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    videoStripLayout_->insertWidget(0, localVideoWidget_);
    localVideoWidget_->setVisible(false);

    // Отдельная плитка для локального превью демонстрации экрана (issue
    // #185) — камера и демонстрация экрана теперь независимы и могут
    // быть видны одновременно, поэтому один общий QVideoWidget на оба
    // источника больше не подходит (кто последний прислал кадр, тот и
    // виден).
    localScreenShareVideoWidget_ = new QVideoWidget(videoStrip_);
    localScreenShareVideoWidget_->setObjectName(QStringLiteral("localScreenShareVideoWidget"));
    localScreenShareVideoWidget_->setMinimumSize(kVideoTileSize, kVideoTileSize);
    localScreenShareVideoWidget_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    videoStripLayout_->insertWidget(1, localScreenShareVideoWidget_);
    localScreenShareVideoWidget_->setVisible(false);

    videoStrip_->setVisible(false);

    rootLayout->addLayout(controlsRow);
    rootLayout->addWidget(callParticipantsLabel_);
    rootLayout->addWidget(videoStrip_, /*stretch=*/1);
}

void CallWindow::setMuted(bool muted) {
    muteToggleButton_->setText(muted ? tr("Unmute") : tr("Mute"));
}

void CallWindow::setVideoEnabled(bool enabled) {
    videoToggleButton_->setText(enabled ? tr("Disable Video") : tr("Enable Video"));
    videoActive_ = enabled;
    localVideoWidget_->setVisible(enabled);
    updateVideoStripVisibility();
}

void CallWindow::setScreenShareEnabled(bool enabled) {
    screenShareToggleButton_->setText(enabled ? tr("Stop Sharing") : tr("Share Screen"));
    screenShareActive_ = enabled;
    localScreenShareVideoWidget_->setVisible(enabled);
    updateVideoStripVisibility();
}

void CallWindow::updateVideoStripVisibility() {
    videoStrip_->setVisible(videoActive_ || screenShareActive_ || !remoteVideoTiles_.isEmpty());
}

void CallWindow::setCallParticipants(const QStringList& participants) {
    if (participants.isEmpty()) {
        callParticipantsLabel_->setVisible(false);
        return;
    }
    callParticipantsLabel_->setText(tr("In call: %1").arg(participants.join(QStringLiteral(", "))));
    callParticipantsLabel_->setVisible(true);
}

void CallWindow::showRemoteVideoFrame(const QString& peerLogin, const QImage& frame, bool isScreenShare) {
    const QString key = remoteTileKey(peerLogin, isScreenShare);
    QLabel* tile = remoteVideoTiles_.value(key, nullptr);
    if (tile == nullptr) {
        tile = new QLabel(videoStrip_);
        tile->setObjectName(QStringLiteral("remoteVideoTile"));
        tile->setMinimumSize(kVideoTileSize, kVideoTileSize);
        tile->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        tile->setScaledContents(true);
        videoStripLayout_->addWidget(tile);
        remoteVideoTiles_.insert(key, tile);
    }
    tile->setPixmap(QPixmap::fromImage(frame));
    videoStrip_->setVisible(true);
}

void CallWindow::removeRemoteVideo(const QString& peerLogin, bool isScreenShare) {
    QLabel* tile = remoteVideoTiles_.take(remoteTileKey(peerLogin, isScreenShare));
    if (tile == nullptr) {
        return;
    }
    delete tile;
    updateVideoStripVisibility();
}

void CallWindow::resetForNewCall() {
    for (QLabel* tile : std::as_const(remoteVideoTiles_)) {
        delete tile;
    }
    remoteVideoTiles_.clear();
    setCallParticipants({});
}

}  // namespace devicehub
