#pragma once

#include <QWidget>

class QPaintEvent;

namespace devicehub {

/**
 * @brief Rounded background container for one message bubble's
 *        content — paints its own fill directly with QPainter rather
 *        than relying on QSS, so the corner radius can be derived
 *        from the current font size (em-relative) instead of a fixed
 *        pixel constant baked into a stylesheet.
 *
 * Own messages (@p isOwnMessage true) get the app's green accent
 * gradient; everyone else's get a neutral dark surface color. Content
 * (author/time/body labels) is added as normal child widgets in a
 * layout set on this widget by the caller (ChatMessageRow) — this
 * class only owns the background.
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
