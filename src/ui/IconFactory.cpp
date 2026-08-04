#include "ui/IconFactory.h"

#include <QApplication>
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

}  // namespace devicehub::ui_icons
