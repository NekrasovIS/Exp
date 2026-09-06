#pragma once

#include <QPoint>
#include <QSize>
#include <QWidget>

class QEvent;
class QObject;
class QResizeEvent;
class QShowEvent;

namespace devicehub {

/**
 * @brief Обёртка над произвольным виджетом содержимого (локальное
 *        превью камеры/экрана, плитка удалённого видео) — issue #185,
 *        последняя часть его рекомендованного разбиения: свободно
 *        перетаскиваемая и растягиваемая мышью плитка внутри canvas
 *        CallWindow::videoStrip_, вместо фиксированной строки
 *        QHBoxLayout, где место каждой плитки решал сам layout.
 *
 * Не знает, что именно внутри — QVideoWidget (локальные превью) или
 * QLabel (удалённые плитки, issue #91): content() заполняет собой всю
 * плитку, а маленький захват-ручка в правом нижнем углу лежит поверх
 * него отдельным дочерним виджетом (grip_), а не рисуется поверх через
 * paintEvent() — content_ обычно композитится поверх обычной отрисовки
 * QPainter родителя (особенно QVideoWidget), так что "нарисовать
 * поверх" здесь не сработало бы. Перетаскивание и resize реализованы
 * через installEventFilter() на content_/grip_ (тот же приём, что уже
 * использует FooterBar для кликабельного avatarLabel_), а не
 * переопределением mousePressEvent() у самой плитки — иначе события
 * мыши перехватывал бы только тот из двух виджетов, что реально
 * находится под курсором, и до плитки они бы не доходили вовсе.
 */
class DraggableVideoTile : public QWidget {
    Q_OBJECT

public:
    /// @p content становится дочерним виджетом этой плитки — вызывающий
    /// код передаёт владение (обычный QObject parent-ownership), сам
    /// больше не должен менять его geometry напрямую (resizeEvent()
    /// этой плитки делает это сам).
    explicit DraggableVideoTile(QWidget* content, QWidget* parent = nullptr);

    [[nodiscard]] QWidget* content() const { return content_; }
    /// Захват-ручка в правом нижнем углу — публична только ради тестов
    /// (нужно послать ей синтетическое QMouseEvent), сама плитка её
    /// геометрией и стилем управляет полностью самостоятельно.
    [[nodiscard]] QWidget* resizeGrip() const { return grip_; }

protected:
    void resizeEvent(QResizeEvent* event) override;
    /// Qt откладывает доставку resizeEvent() виджету, пока он скрыт
    /// (собственный size()/geometry() при этом всё равно обновляются
    /// сразу — эффект виден только в отложенной раскладке content_/
    /// grip_) — CallWindow строит и сразу же resize()-ит новые плитки
    /// ещё до первого показа окна звонка (`show()`), поэтому без этого
    /// перехвата content_/grip_ остались бы на месте вплоть до первого
    /// показа именно этой плитки, а не окна целиком.
    void showEvent(QShowEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    /// Расставляет content_/grip_ по текущим width()/height() этой
    /// плитки — общая часть resizeEvent()/showEvent() (см. их
    /// doc-комментарии на предмет того, почему их два, а не один).
    void layoutChildren();

    /// Общая часть обработки press/move/release и для перетаскивания
    /// (watched == content_), и для resize (watched == grip_) — какое
    /// из двух состояний обновлять, разбирает сам по @p watched.
    bool handleMouseEvent(QObject* watched, QEvent* event);

    QWidget* content_ = nullptr;
    QWidget* grip_ = nullptr;
    bool dragging_ = false;
    bool resizing_ = false;
    /// Позиция курсора (в координатах content_) в момент начала
    /// перетаскивания — move() на каждое движение мыши двигает плитку
    /// так, чтобы эта точка курсора оставалась над тем же местом
    /// плитки, а не "прыгала" к точной позиции клика.
    QPoint dragStartCursorPos_;
    /// Размер плитки и глобальная позиция курсора на момент начала
    /// resize — новый размер каждый раз считается от них плюс текущая
    /// дельта, а не накопительно от предыдущего кадра, чтобы не
    /// копилась ошибка округления за много мелких событий подряд.
    QSize resizeStartSize_;
    QPoint resizeStartGlobalCursorPos_;
};

}  // namespace devicehub
