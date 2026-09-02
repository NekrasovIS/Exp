#include "ui/ChatBubble.h"

#include <QColor>
#include <QFontMetricsF>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>

#include "ui/Theme.h"

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
        gradient.setColorAt(0, QColor(ui_theme::kAccentGradientStart));
        gradient.setColorAt(1, QColor(ui_theme::kAccentGradientEnd));
        painter.fillPath(path, gradient);
    } else {
        painter.fillPath(path, QColor(ui_theme::kBubbleOtherBackground));
    }
}

}  // namespace devicehub
