#pragma once

#include <QWidget>

class QPushButton;

namespace devicehub {

/**
 * @brief Компактное плавающее окно со свёрнутыми мини-плитками звонка
 *        (issue #215) — то, что MainWindow показывает вместо CallWindow,
 *        пока звонок свёрнут, чтобы активное видео (своя камера/
 *        демонстрация экрана, видео удалённых участников) оставалось
 *        видимым в компактном виде, как picture-in-picture.
 *
 * Сама не владеет никакими DraggableVideoTile и не знает про звонок —
 * вся эта логика (какие плитки существуют, куда их переносить, каким
 * размером) остаётся в CallWindow (см. её detachTilesTo()/
 * reattachTiles()), это окно только предоставляет canvas() — обычный
 * QWidget без layout'а, куда CallWindow репарентит свои плитки, и
 * кнопку возврата. Тот же принцип "чистого представления", что и у
 * CallWindow/ChatView — MainWindow решает, когда это окно показать/
 * скрыть, оно само не решает.
 */
class FloatingCallTilesOverlay : public QWidget {
    Q_OBJECT

public:
    explicit FloatingCallTilesOverlay(QWidget* parent = nullptr);

    /// Canvas без layout'а — DraggableVideoTile-плитки внутри него
    /// позиционируются и перетаскиваются так же, как и в
    /// CallWindow::videoStrip_ (тот же класс плиток, просто другой
    /// текущий родитель).
    [[nodiscard]] QWidget* canvas() const { return canvas_; }
    [[nodiscard]] QPushButton* restoreButton() const { return restoreButton_; }

signals:
    /// Клик по кнопке возврата — MainWindow вызывает
    /// CallWindow::reattachTiles() и показывает CallWindow обратно.
    void restoreRequested();

private:
    QWidget* canvas_ = nullptr;
    QPushButton* restoreButton_ = nullptr;
};

}  // namespace devicehub
