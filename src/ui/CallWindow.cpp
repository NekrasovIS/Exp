#include "ui/CallWindow.h"

#include <QCloseEvent>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>
#include <QVideoWidget>

#include <cmath>
#include <utility>

#include "ui/Theme.h"

namespace devicehub {

namespace {
constexpr int kVideoTileSize = 160;
/// Компактный размер плитки, пока звонок свёрнут (issue #215) — заметно
/// меньше, чтобы FloatingCallTilesOverlay реально выглядел как
/// picture-in-picture, а не уменьшенная копия CallWindow.
constexpr int kMiniVideoTileSize = 96;
/// Issue #312 — тот же порядок величины, что kTypingIndicatorHideMs у
/// ChatView, только короче: реакция — мгновенный жест, а не состояние,
/// которое имеет смысл держать на экране несколько секунд подряд.
constexpr int kReactionFeedHideMs = 2500;

/// Число колонок грида для @p tileCount активных плиток (issue #437) —
/// ближайший верхний квадратный корень, чтобы грид оставался примерно
/// квадратным при любом количестве участников, а не растягивался в одну
/// длинную строку/столбец.
int gridColumnsFor(int tileCount) {
    if (tileCount <= 1) {
        return 1;
    }
    return static_cast<int>(std::ceil(std::sqrt(static_cast<double>(tileCount))));
}

/// Фиксированный набор быстрых реакций (issue #312) — эмодзи вне
/// Basic Multilingual Plane (нужен суррогатная пара в UTF-16) записаны
/// экранированными Unicode-литералами `\U0001F...`, тот же приём, что
/// videoPlaceholder's "\U0001F3AC" в ChatMessageRow.cpp; ❤️ (U+2764
/// U+FE0F) — оба кодпойнта уже в BMP, суррогатная пара не нужна, можно
/// прямо в исходнике.
QList<QString> reactionEmojis() {
    return {QStringLiteral("\U0001F44D"), QStringLiteral("❤️"), QStringLiteral("\U0001F602"),
            QStringLiteral("\U0001F389"), QStringLiteral("\U0001F44F")};
}

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

    // Issue #312 — фиксированный набор быстрых реакций, своя строка под
    // controlsRow, а не смешаны с ним: это не переключатели состояния
    // звонка, как остальные кнопки выше, а одноразовые жесты.
    auto* reactionsRow = new QHBoxLayout;
    reactionsRow->setSpacing(ui_theme::kSpacingSm);
    for (const QString& emoji : reactionEmojis()) {
        auto* reactionButton = new QPushButton(emoji, this);
        reactionButton->setProperty("reactionEmoji", emoji);
        connect(reactionButton, &QPushButton::clicked, this,
                [this, emoji]() { emit reactionRequested(emoji); });
        reactionButtons_.append(reactionButton);
        reactionsRow->addWidget(reactionButton);
    }
    reactionsRow->addStretch(1);

    reactionFeedLabel_ = new QLabel(this);
    // Issue #450 — свой objectName вместо общего "mutedDescription": этот
    // текст — лёгкий тост поверх окна звонка (полупрозрачные видео-плитки
    // и разный фон под ним), тогда как "mutedDescription" расчитан на
    // контраст с непрозрачным фоном панели настроек/чата — тусклого цвета
    // не хватало, чтобы читаться поверх звонка.
    reactionFeedLabel_->setObjectName(QStringLiteral("callReactionFeed"));
    reactionFeedLabel_->setVisible(false);
    reactionFeedHideTimer_ = new QTimer(this);
    reactionFeedHideTimer_->setSingleShot(true);
    reactionFeedHideTimer_->setInterval(kReactionFeedHideMs);
    connect(reactionFeedHideTimer_, &QTimer::timeout, this, [this]() { reactionFeedLabel_->setVisible(false); });

    callParticipantsLabel_ = new QLabel(this);
    callParticipantsLabel_->setObjectName(QStringLiteral("mutedDescription"));
    callParticipantsLabel_->setWordWrap(true);
    callParticipantsLabel_->setVisible(false);

    // Grid-хост (issue #437) — колонки/строки пересчитываются от
    // текущего числа активных плиток в relayoutVideoGrid(), сам layout
    // создаётся лениво через ensureGridLayout() при первом обращении.
    videoStrip_ = new QWidget(this);
    videoStrip_->setVisible(false);
    tileHost_ = videoStrip_;
    currentTileSize_ = kVideoTileSize;
    ensureGridLayout(videoStrip_);

    localVideoWidget_ = new QVideoWidget(this);
    localVideoWidget_->setObjectName(QStringLiteral("localVideoWidget"));
    // Issue #450 — QVideoWidget — plain QWidget, не QFrame/QLabel, так
    // что без этого атрибута QSS-рамка ниже ([videoTile="true"]) вообще
    // не нарисуется (тот же приём, что и у DraggableVideoTile's
    // resizeGrip_ до issue #437). Свойство, а не objectName, — обе
    // локальные плитки уже используют objectName как уникальный
    // идентификатор для тестов/логики, "videoTile" здесь — это класс
    // общего вида, а не имя конкретного виджета (тот же приём, что и
    // [sectionTitle="true"]/[chatAuthor="true"] в Theme.cpp).
    localVideoWidget_->setAttribute(Qt::WA_StyledBackground, true);
    localVideoWidget_->setProperty("videoTile", true);
    localVideoWidget_->setVisible(false);

    // Отдельный виджет для локального превью демонстрации экрана (issue
    // #185) — камера и демонстрация экрана теперь независимы и могут
    // быть видны одновременно, поэтому один общий QVideoWidget на оба
    // источника больше не подходит (кто последний прислал кадр, тот и
    // виден).
    localScreenShareVideoWidget_ = new QVideoWidget(this);
    localScreenShareVideoWidget_->setObjectName(QStringLiteral("localScreenShareVideoWidget"));
    localScreenShareVideoWidget_->setAttribute(Qt::WA_StyledBackground, true);
    localScreenShareVideoWidget_->setProperty("videoTile", true);
    localScreenShareVideoWidget_->setVisible(false);

    rootLayout->addLayout(controlsRow);
    rootLayout->addLayout(reactionsRow);
    rootLayout->addWidget(reactionFeedLabel_);
    rootLayout->addWidget(callParticipantsLabel_);
    rootLayout->addWidget(videoStrip_, /*stretch=*/1);
}

void CallWindow::showReaction(const QString& login, const QString& emoji) {
    reactionFeedLabel_->setText(QStringLiteral("%1 %2").arg(login, emoji));
    reactionFeedLabel_->setVisible(true);
    reactionFeedHideTimer_->start();
}

void CallWindow::setMuted(bool muted) {
    muteToggleButton_->setText(muted ? tr("Unmute") : tr("Mute"));
}

void CallWindow::setVideoEnabled(bool enabled) {
    videoToggleButton_->setText(enabled ? tr("Disable Video") : tr("Enable Video"));
    videoActive_ = enabled;
    relayoutVideoGrid();
}

void CallWindow::setScreenShareEnabled(bool enabled) {
    screenShareToggleButton_->setText(enabled ? tr("Stop Sharing") : tr("Share Screen"));
    screenShareActive_ = enabled;
    relayoutVideoGrid();
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

QGridLayout* CallWindow::ensureGridLayout(QWidget* host) {
    auto* grid = qobject_cast<QGridLayout*>(host->layout());
    if (grid == nullptr) {
        grid = new QGridLayout(host);
        grid->setSpacing(ui_theme::kSpacingSm);
    }
    return grid;
}

void CallWindow::relayoutVideoGrid() {
    QGridLayout* grid = ensureGridLayout(tileHost_);
    // Убираем все ячейки (не виджеты — QLayout::takeAt() не трогает
    // родителя), чтобы заново расставить их с нуля по актуальному
    // набору и числу колонок ниже, а не пытаться вычислить дельту.
    // takeAt() передаёт вызывающему владение самим QLayoutItem-обёрткой
    // (не виджетом внутри неё) — без explicit delete здесь была бы
    // утечка QLayoutItem на каждый вызов relayoutVideoGrid().
    while (grid->count() > 0) {
        delete grid->takeAt(0);
    }

    QList<QWidget*> activeWidgets;
    if (videoActive_) {
        activeWidgets.append(localVideoWidget_);
    }
    if (screenShareActive_) {
        activeWidgets.append(localScreenShareVideoWidget_);
    }
    for (QLabel* tile : std::as_const(remoteVideoTiles_)) {
        activeWidgets.append(tile);
    }

    const int columns = gridColumnsFor(activeWidgets.size());
    for (int i = 0; i < activeWidgets.size(); ++i) {
        QWidget* widget = activeWidgets.at(i);
        widget->setMinimumSize(currentTileSize_, currentTileSize_);
        grid->addWidget(widget, i / columns, i % columns);
    }
    // QGridLayout::addWidget() зовёт QWidget::setParent(), когда родитель
    // меняется, а тот неявно скрывает виджет как побочный эффект смены
    // родителя (issue #287, тот же эффект актуален и для грида) —
    // восстанавливаем реальную видимость явно, а не полагаемся на то,
    // что addWidget() её не тронула.
    for (QWidget* widget : std::as_const(activeWidgets)) {
        widget->setVisible(true);
    }
    localVideoWidget_->setVisible(videoActive_);
    localScreenShareVideoWidget_->setVisible(screenShareActive_);
    updateVideoStripVisibility();
}

void CallWindow::showRemoteVideoFrame(const QString& peerLogin, const QImage& frame, bool isScreenShare) {
    const QString key = remoteTileKey(peerLogin, isScreenShare);
    QLabel* label = remoteVideoTiles_.value(key, nullptr);
    if (label == nullptr) {
        label = new QLabel(this);
        label->setObjectName(QStringLiteral("remoteVideoTile"));
        label->setScaledContents(true);
        remoteVideoTiles_.insert(key, label);
    }
    label->setPixmap(QPixmap::fromImage(frame));
    relayoutVideoGrid();
}

void CallWindow::removeRemoteVideo(const QString& peerLogin, bool isScreenShare) {
    QLabel* label = remoteVideoTiles_.take(remoteTileKey(peerLogin, isScreenShare));
    if (label == nullptr) {
        return;
    }
    delete label;
    relayoutVideoGrid();
}

void CallWindow::resetForNewCall() {
    // Возвращает tileHost_/размер к нормальному состоянию первым делом
    // (issue #215) — предыдущий звонок мог закончиться, пока был свёрнут
    // в FloatingCallTilesOverlay; новый звонок не должен унаследовать ни
    // это, ни его мини-размер плиток.
    reattachTiles();
    for (QLabel* tile : std::as_const(remoteVideoTiles_)) {
        delete tile;
    }
    remoteVideoTiles_.clear();
    relayoutVideoGrid();
    setCallParticipants({});
}

void CallWindow::detachTilesTo(QWidget* newParent) {
    tileHost_ = newParent;
    currentTileSize_ = kMiniVideoTileSize;
    relayoutVideoGrid();
}

void CallWindow::reattachTiles() {
    tileHost_ = videoStrip_;
    currentTileSize_ = kVideoTileSize;
    relayoutVideoGrid();
}

void CallWindow::closeEvent(QCloseEvent* event) {
    event->ignore();
    emit minimizeRequested();
}

}  // namespace devicehub
