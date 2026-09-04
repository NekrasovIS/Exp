#include "ui/DraggableVideoTile.h"

#include <QColor>
#include <QEvent>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QShowEvent>

#include "ui/Theme.h"

namespace devicehub {

namespace {
constexpr int kResizeGripSize = 16;
/// Меньше kVideoTileSize (160px, см. CallWindow.cpp) — растягивать
/// плитку вниз до её изначального размера, а не до нуля, но всё же
/// позволить реально уменьшить её, а не только увеличивать.
constexpr int kMinTileSize = 96;
}  // namespace

DraggableVideoTile::DraggableVideoTile(QWidget* content, QWidget* parent) : QWidget(parent), content_(content) {
    content_->setParent(this);
    content_->setCursor(Qt::SizeAllCursor);
    content_->installEventFilter(this);

    // Отдельный дочерний виджет, а не paintEvent() поверх content_ —
    // content_ обычно рендерится композитно (особенно QVideoWidget) и
    // перекрыл бы обычную QPainter-отрисовку родителя. Полупрозрачная
    // заливка акцентным цветом (тот же, что и остальной UI) со
    // скруглением только внутреннего угла — читается как "потяни за
    // уголок", не как отдельная декоративная деталь.
    grip_ = new QWidget(this);
    grip_->setObjectName(QStringLiteral("videoTileResizeGrip"));
    grip_->setFixedSize(kResizeGripSize, kResizeGripSize);
    grip_->setCursor(Qt::SizeFDiagCursor);
    grip_->setAttribute(Qt::WA_StyledBackground, true);
    const QColor accent(ui_theme::kAccentGradientStart);
    grip_->setStyleSheet(QStringLiteral("background-color: rgba(%1, %2, %3, 150); border-top-left-radius: 6px;")
                              .arg(accent.red())
                              .arg(accent.green())
                              .arg(accent.blue()));
    grip_->installEventFilter(this);
    grip_->raise();

    setMinimumSize(kMinTileSize, kMinTileSize);
}

void DraggableVideoTile::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    layoutChildren();
}

void DraggableVideoTile::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    layoutChildren();
}

void DraggableVideoTile::layoutChildren() {
    content_->setGeometry(0, 0, width(), height());
    grip_->move(width() - kResizeGripSize, height() - kResizeGripSize);
    grip_->raise();
}

bool DraggableVideoTile::eventFilter(QObject* watched, QEvent* event) {
    if (watched != content_ && watched != grip_) {
        return QWidget::eventFilter(watched, event);
    }
    switch (event->type()) {
        case QEvent::MouseButtonPress:
        case QEvent::MouseMove:
        case QEvent::MouseButtonRelease:
            if (handleMouseEvent(watched, event)) {
                return true;
            }
            break;
        default:
            break;
    }
    return QWidget::eventFilter(watched, event);
}

bool DraggableVideoTile::handleMouseEvent(QObject* watched, QEvent* event) {
    auto* mouseEvent = static_cast<QMouseEvent*>(event);

    if (event->type() == QEvent::MouseButtonPress) {
        if (mouseEvent->button() != Qt::LeftButton) {
            return false;
        }
        if (watched == grip_) {
            resizing_ = true;
            resizeStartSize_ = size();
            resizeStartGlobalCursorPos_ = mouseEvent->globalPosition().toPoint();
        } else {
            dragging_ = true;
            // Позиция мыши относительно content_ совпадает с позицией
            // относительно этой плитки — content_ всегда стоит ровно в
            // (0, 0) её локальных координат (см. resizeEvent()).
            dragStartCursorPos_ = mouseEvent->position().toPoint();
        }
        return true;
    }

    if (event->type() == QEvent::MouseMove) {
        if (resizing_) {
            const QPoint delta = mouseEvent->globalPosition().toPoint() - resizeStartGlobalCursorPos_;
            const QSize newSize = (resizeStartSize_ + QSize(delta.x(), delta.y())).expandedTo(QSize(kMinTileSize, kMinTileSize));
            resize(newSize);
            return true;
        }
        if (dragging_) {
            const QPoint delta = mouseEvent->position().toPoint() - dragStartCursorPos_;
            move(pos() + delta);
            return true;
        }
        return false;
    }

    if (event->type() == QEvent::MouseButtonRelease) {
        dragging_ = false;
        resizing_ = false;
        return true;
    }

    return false;
}

}  // namespace devicehub
