#pragma once

#include <QWidget>

class QPaintEvent;

namespace devicehub {

/**
 * @brief Контейнер со скруглённым фоном для содержимого одного
 *        пузыря сообщения — рисует свою заливку напрямую через
 *        QPainter, а не полагается на QSS, чтобы радиус скругления
 *        можно было выводить из текущего размера шрифта (в em),
 *        а не из фиксированной пиксельной константы, зашитой в
 *        таблицу стилей.
 *
 * Собственные сообщения (@p isOwnMessage true) получают зелёный
 * акцентный градиент приложения; чужие — нейтральный тёмный цвет
 * поверхности. Содержимое (метки автора/времени/текста) добавляется
 * как обычные дочерние виджеты в layout, который вызывающий код
 * (ChatMessageRow) устанавливает на этом виджете — сам класс отвечает
 * только за фон.
 */
class ChatBubble : public QWidget {
    Q_OBJECT

public:
    explicit ChatBubble(bool isOwnMessage, QWidget* parent = nullptr);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    bool isOwnMessage_ = false;
};

}  // namespace devicehub
