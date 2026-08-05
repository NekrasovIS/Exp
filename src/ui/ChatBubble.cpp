#include "ui/ChatBubble.h"

#include <QColor>
#include <QFontMetricsF>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>

namespace devicehub {

namespace {
constexpr qreal kRadiusEm = 0.6;
}  // namespace

ChatBubble::ChatBubble(bool isOwnMessage, QWidget* parent) : QWidget(parent), isOwnMessage_(isOwnMessage) {
}

void ChatBubble::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const qreal radius = QFontMetricsF(font()).height() * kRadiusEm;
    QPainterPath path;
    path.addRoundedRect(rect(), radius, radius);

    if (isOwnMessage_) {
        QLinearGradient gradient(rect().topLeft(), rect().bottomRight());
        gradient.setColorAt(0, QColor("#34d399"));
        gradient.setColorAt(1, QColor("#059669"));
        painter.fillPath(path, gradient);
    } else {
        painter.fillPath(path, QColor("#2a2d31"));
    }
}

}  // namespace devicehub
