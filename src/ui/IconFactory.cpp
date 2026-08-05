#include "ui/IconFactory.h"

#include <QApplication>
#include <QColor>
#include <QFont>
#include <QLinearGradient>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QStyle>

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
    gradient.setColorAt(0, QColor("#34d399"));
    gradient.setColorAt(1, QColor("#059669"));
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

}  // namespace devicehub::ui_icons
