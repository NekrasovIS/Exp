#pragma once

#include <QWidget>

class QLabel;
class QTimer;

namespace devicehub {

/**
 * @brief Временный немодальный баннер обратной связи, плавающий над
 *        своим родителем (отступ сверху, на всю ширину) — используется
 *        для сообщений об успехе/ошибке/информации CRUD-операций,
 *        которые легко пропустить в строке QMainWindow::statusBar().
 *
 * Позиционирует себя через фильтр событий на родителе (перепозициони-
 * руется при изменении размера), а не участвует в layout'е родителя,
 * поскольку ему нужно плавать поверх всего содержимого, которое там уже
 * есть.
 */
class ToastBanner : public QWidget {
    Q_OBJECT

public:
    enum class Variant { kSuccess, kError, kInfo };

    /// @p parent — виджет, над которым плавает баннер (например,
    /// ChatView) — не должен быть null.
    explicit ToastBanner(QWidget* parent);

    /// Показывает @p text в течение @p timeoutMs миллисекунд,
    /// оформленный согласно @p variant. Повторный вызов, пока баннер уже
    /// виден, перезапускает таймаут с новым текстом/вариантом.
    void showMessage(const QString& text, Variant variant, int timeoutMs);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void reposition();

    QLabel* label_ = nullptr;
    QTimer* hideTimer_ = nullptr;
};

}  // namespace devicehub
