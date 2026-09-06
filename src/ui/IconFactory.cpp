#include "ui/IconFactory.h"

#include <QApplication>
#include <QColor>
#include <QFont>
#include <QLinearGradient>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QStyle>

#include "ui/Theme.h"

namespace devicehub::ui_icons {

QIcon plusIcon(const QColor& strokeColor) {
    constexpr int kSize = 24;
    constexpr qreal kDevicePixelRatio = 2.0;

    QPixmap pixmap(QSize(kSize, kSize) * kDevicePixelRatio);
    pixmap.setDevicePixelRatio(kDevicePixelRatio);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(strokeColor, 2.4, Qt::SolidLine, Qt::RoundCap));
    painter.drawLine(QPointF(12, 5), QPointF(12, 19));
    painter.drawLine(QPointF(5, 12), QPointF(19, 12));

    return QIcon(pixmap);
}

QIcon refreshIcon() {
    return qApp->style()->standardIcon(QStyle::SP_BrowserReload);
}

QIcon communityAvatarIcon(const QString& label) {
    constexpr int kSize = 40;
    constexpr qreal kDevicePixelRatio = 2.0;
    constexpr qreal kCornerRadius = 12.0;

    QPixmap pixmap(QSize(kSize, kSize) * kDevicePixelRatio);
    pixmap.setDevicePixelRatio(kDevicePixelRatio);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    QLinearGradient gradient(0, 0, kSize, kSize);
    gradient.setColorAt(0, QColor(devicehub::ui_theme::kAccentGradientStart));
    gradient.setColorAt(1, QColor(devicehub::ui_theme::kAccentGradientEnd));
    painter.setPen(Qt::NoPen);
    painter.setBrush(gradient);
    painter.drawRoundedRect(QRectF(0, 0, kSize, kSize), kCornerRadius, kCornerRadius);

    QFont font = painter.font();
    font.setBold(true);
    font.setPointSizeF(14.0);
    painter.setFont(font);
    painter.setPen(Qt::white);
    painter.drawText(QRectF(0, 0, kSize, kSize), Qt::AlignCenter, label);

    return QIcon(pixmap);
}

QIcon friendsIcon(const QColor& strokeColor) {
    constexpr int kSize = 24;
    constexpr qreal kDevicePixelRatio = 2.0;

    QPixmap pixmap(QSize(kSize, kSize) * kDevicePixelRatio);
    pixmap.setDevicePixelRatio(kDevicePixelRatio);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(strokeColor, 1.8, Qt::SolidLine, Qt::RoundCap));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(QPointF(9, 9), 4.0, 4.0);
    painter.drawEllipse(QPointF(16, 10), 3.2, 3.2);
    painter.drawArc(QRectF(3, 14, 12, 8), 0, 180 * 16);
    painter.drawArc(QRectF(12.5, 15, 9, 6.5), 0, 180 * 16);

    return QIcon(pixmap);
}

}  // namespace devicehub::ui_icons
