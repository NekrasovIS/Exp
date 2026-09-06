#include "ui/DraggableVideoTile.h"

#include <gtest/gtest.h>

#include <QApplication>
#include <QLabel>
#include <QMouseEvent>
#include <QWidget>

namespace devicehub {
namespace {

TEST(DraggableVideoTileTest, ContentFillsTheTileAfterResize) {
    // Qt откладывает доставку resizeEvent() скрытым виджетам до их
    // собственного первого show() (не просто show() у родителя — новый
    // ребёнок скрытого-по-умолчанию виджета сам по себе видимым не
    // становится без layout'а, который бы вызвал show() за нас) —
    // size()/geometry() самой плитки при этом обновляются сразу, эффект
    // виден только в отложенной раскладке content_/grip_, для которой и
    // существует showEvent() (см. DraggableVideoTile.h).
    QWidget canvas;
    canvas.show();
    auto* content = new QLabel();
    DraggableVideoTile tile(content, &canvas);
    tile.show();

    tile.resize(200, 150);

    EXPECT_EQ(tile.content(), content);
    EXPECT_EQ(content->geometry(), QRect(0, 0, 200, 150));
}

TEST(DraggableVideoTileTest, ResizeGripSitsInTheBottomRightCorner) {
    QWidget canvas;
    canvas.show();
    auto* content = new QLabel();
    DraggableVideoTile tile(content, &canvas);
    tile.show();

    tile.resize(200, 150);

    const QRect gripGeometry = tile.resizeGrip()->geometry();
    EXPECT_EQ(gripGeometry.right() + 1, 200);
    EXPECT_EQ(gripGeometry.bottom() + 1, 150);
}

TEST(DraggableVideoTileTest, DraggingContentMovesTheTile) {
    QWidget canvas;
    auto* content = new QLabel();
    DraggableVideoTile tile(content, &canvas);
    tile.resize(200, 150);
    tile.move(10, 10);

    QMouseEvent press(QEvent::MouseButtonPress, QPointF(20, 20), QPointF(20, 20), Qt::LeftButton, Qt::LeftButton,
                       Qt::NoModifier);
    QApplication::sendEvent(content, &press);

    QMouseEvent move(QEvent::MouseMove, QPointF(35, 40), QPointF(35, 40), Qt::LeftButton, Qt::LeftButton,
                      Qt::NoModifier);
    QApplication::sendEvent(content, &move);

    // Курсор сдвинулся на (+15, +20) относительно content_ с момента
    // press — плитка должна была сдвинуться ровно на столько же.
    EXPECT_EQ(tile.pos(), QPoint(25, 30));

    QMouseEvent release(QEvent::MouseButtonRelease, QPointF(35, 40), QPointF(35, 40), Qt::LeftButton, Qt::NoButton,
                         Qt::NoModifier);
    QApplication::sendEvent(content, &release);

    // Дальнейшее движение без нового press не должно ничего сдвигать.
    QMouseEvent moveAfterRelease(QEvent::MouseMove, QPointF(100, 100), QPointF(100, 100), Qt::LeftButton,
                                  Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(content, &moveAfterRelease);
    EXPECT_EQ(tile.pos(), QPoint(25, 30));
}

TEST(DraggableVideoTileTest, DraggingResizeGripResizesTheTile) {
    QWidget canvas;
    auto* content = new QLabel();
    DraggableVideoTile tile(content, &canvas);
    tile.resize(200, 150);
    const QPoint tilePosBefore = tile.pos();

    QMouseEvent press(QEvent::MouseButtonPress, QPointF(8, 8), QPointF(500, 500), Qt::LeftButton, Qt::LeftButton,
                       Qt::NoModifier);
    QApplication::sendEvent(tile.resizeGrip(), &press);

    QMouseEvent move(QEvent::MouseMove, QPointF(8, 8), QPointF(540, 560), Qt::LeftButton, Qt::LeftButton,
                      Qt::NoModifier);
    QApplication::sendEvent(tile.resizeGrip(), &move);

    // Глобальный курсор сдвинулся на (+40, +60) с момента press —
    // размер плитки должен был вырасти ровно на столько же, позиция
    // (левый верхний угол) при этом не меняется — resize растёт только
    // вниз-вправо.
    EXPECT_EQ(tile.size(), QSize(240, 210));
    EXPECT_EQ(tile.pos(), tilePosBefore);
}

TEST(DraggableVideoTileTest, ResizeNeverShrinksBelowTheMinimumSize) {
    QWidget canvas;
    auto* content = new QLabel();
    DraggableVideoTile tile(content, &canvas);
    tile.resize(120, 120);

    QMouseEvent press(QEvent::MouseButtonPress, QPointF(8, 8), QPointF(0, 0), Qt::LeftButton, Qt::LeftButton,
                       Qt::NoModifier);
    QApplication::sendEvent(tile.resizeGrip(), &press);

    QMouseEvent move(QEvent::MouseMove, QPointF(8, 8), QPointF(-500, -500), Qt::LeftButton, Qt::LeftButton,
                      Qt::NoModifier);
    QApplication::sendEvent(tile.resizeGrip(), &move);

    EXPECT_GE(tile.width(), tile.minimumWidth());
    EXPECT_GE(tile.height(), tile.minimumHeight());
}

}  // namespace
}  // namespace devicehub
