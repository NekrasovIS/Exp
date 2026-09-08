#include "ui/CallWindow.h"

#include <QCloseEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QVBoxLayout>
#include <QVideoWidget>

#include <utility>

#include "ui/DraggableVideoTile.h"
#include "ui/Theme.h"

namespace devicehub {

namespace {
constexpr int kVideoTileSize = 160;
/// Компактный размер плитки, пока звонок свёрнут (issue #215) — заметно
/// меньше, чтобы FloatingCallTilesOverlay реально выглядел как
/// picture-in-picture, а не уменьшенная копия CallWindow.
constexpr int kMiniVideoTileSize = 96;
constexpr int kCascadeStep = 28;
constexpr int kCascadeMaxSteps = 8;

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

    minimizeButton_ = new QPushButton(tr("Minimize"), this);
    minimizeButton_->setObjectName(QStringLiteral("minimizeCallButton"));
    connect(minimizeButton_, &QPushButton::clicked, this, &CallWindow::minimizeRequested);

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
    controlsRow->addWidget(minimizeButton_);
    controlsRow->addWidget(leaveCallButton_);

    callParticipantsLabel_ = new QLabel(this);
    callParticipantsLabel_->setObjectName(QStringLiteral("mutedDescription"));
    callParticipantsLabel_->setWordWrap(true);
    callParticipantsLabel_->setVisible(false);

    // Canvas без layout'а (issue #185) — каждая плитка внутри свободно
    // перетаскивается/растягивается мышью (DraggableVideoTile), а не
    // выстраивается сама по QHBoxLayout, как раньше.
    videoStrip_ = new QWidget(this);
    videoStrip_->setVisible(false);
    tileHost_ = videoStrip_;
    currentTileSize_ = kVideoTileSize;

    localVideoWidget_ = new QVideoWidget();
    localVideoWidget_->setObjectName(QStringLiteral("localVideoWidget"));
    localCameraTile_ = new DraggableVideoTile(localVideoWidget_, videoStrip_);
    localCameraTile_->resize(kVideoTileSize, kVideoTileSize);
    localCameraTile_->move(ui_theme::kSpacingSm, ui_theme::kSpacingSm);
    localCameraTile_->setVisible(false);

    // Отдельная плитка для локального превью демонстрации экрана (issue
    // #185) — камера и демонстрация экрана теперь независимы и могут
    // быть видны одновременно, поэтому один общий QVideoWidget на оба
    // источника больше не подходит (кто последний прислал кадр, тот и
    // виден).
    localScreenShareVideoWidget_ = new QVideoWidget();
    localScreenShareVideoWidget_->setObjectName(QStringLiteral("localScreenShareVideoWidget"));
    localScreenShareTile_ = new DraggableVideoTile(localScreenShareVideoWidget_, videoStrip_);
    localScreenShareTile_->resize(kVideoTileSize, kVideoTileSize);
    localScreenShareTile_->move(2 * ui_theme::kSpacingSm + kVideoTileSize, ui_theme::kSpacingSm);
    localScreenShareTile_->setVisible(false);

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
    localCameraTile_->setVisible(enabled);
    updateVideoStripVisibility();
}

void CallWindow::setScreenShareEnabled(bool enabled) {
    screenShareToggleButton_->setText(enabled ? tr("Stop Sharing") : tr("Share Screen"));
    screenShareActive_ = enabled;
    localScreenShareTile_->setVisible(enabled);
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

QPoint CallWindow::nextTileCascadePosition() {
    const int step = nextTileCascadeIndex_ % kCascadeMaxSteps;
    ++nextTileCascadeIndex_;
    const int baseY = ui_theme::kSpacingSm + currentTileSize_ + ui_theme::kSpacingSm;
    return {ui_theme::kSpacingSm + step * kCascadeStep, baseY + step * kCascadeStep};
}

void CallWindow::placeTile(DraggableVideoTile* tile) {
    // Видимость намеренно не трогает — вызывающая сторона решает: у
    // новой удалённой плитки (showRemoteVideoFrame()) она всегда
    // становится видимой, а у локальных камеры/демонстрации экрана при
    // detachTilesTo()/reattachTiles() (issue #215) нужно сохранить их
    // текущее состояние enabled/disabled, а не форсировать true.
    tile->setParent(tileHost_);
    tile->resize(currentTileSize_, currentTileSize_);
    tile->move(nextTileCascadePosition());
}

void CallWindow::showRemoteVideoFrame(const QString& peerLogin, const QImage& frame, bool isScreenShare) {
    const QString key = remoteTileKey(peerLogin, isScreenShare);
    DraggableVideoTile* tile = remoteVideoTiles_.value(key, nullptr);
    QLabel* label = nullptr;
    if (tile == nullptr) {
        label = new QLabel();
        label->setObjectName(QStringLiteral("remoteVideoTile"));
        label->setScaledContents(true);
        tile = new DraggableVideoTile(label, tileHost_);
        placeTile(tile);
        remoteVideoTiles_.insert(key, tile);
    } else {
        label = qobject_cast<QLabel*>(tile->content());
    }
    tile->setVisible(true);
    label->setPixmap(QPixmap::fromImage(frame));
    updateVideoStripVisibility();
}

void CallWindow::removeRemoteVideo(const QString& peerLogin, bool isScreenShare) {
    DraggableVideoTile* tile = remoteVideoTiles_.take(remoteTileKey(peerLogin, isScreenShare));
    if (tile == nullptr) {
        return;
    }
    delete tile;
    updateVideoStripVisibility();
}

void CallWindow::resetForNewCall() {
    // Возвращает tileHost_/размер к нормальному состоянию первым делом
    // (issue #215) — предыдущий звонок мог закончиться, пока был свёрнут
    // в FloatingCallTilesOverlay; новый звонок не должен унаследовать ни
    // это, ни его мини-размер плиток.
    reattachTiles();
    for (DraggableVideoTile* tile : std::as_const(remoteVideoTiles_)) {
        delete tile;
    }
    remoteVideoTiles_.clear();
    setCallParticipants({});
}

void CallWindow::detachTilesTo(QWidget* newParent) {
    tileHost_ = newParent;
    currentTileSize_ = kMiniVideoTileSize;
    relocateAllTiles();
}

void CallWindow::reattachTiles() {
    tileHost_ = videoStrip_;
    currentTileSize_ = kVideoTileSize;
    relocateAllTiles();
}

void CallWindow::relocateAllTiles() {
    nextTileCascadeIndex_ = 0;
    placeTile(localCameraTile_);
    placeTile(localScreenShareTile_);
    for (DraggableVideoTile* tile : std::as_const(remoteVideoTiles_)) {
        placeTile(tile);
        // В отличие от локальных плиток выше (сохраняют свою видимость,
        // см. doc-комментарий placeTile()), удалённая плитка в
        // remoteVideoTiles_ по инварианту этого класса всегда должна
        // быть видна, пока существует (см. removeRemoteVideo()) —
        // явно восстанавливаем на случай, если что-то её скрыло.
        tile->setVisible(true);
    }
    updateVideoStripVisibility();
}

void CallWindow::closeEvent(QCloseEvent* event) {
    event->ignore();
    emit minimizeRequested();
}

}  // namespace devicehub
