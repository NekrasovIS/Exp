#include "ui/IconFactory.h"

#include <QApplication>
#include <QColor>
#include <QFont>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
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
    // Круг, а не скруглённый квадрат — визуальный язык списка серверов
    // Discord для аватара сообщества.
    painter.drawEllipse(QRectF(0, 0, kSize, kSize));

    QFont font = painter.font();
    font.setBold(true);
    font.setPointSizeF(14.0);
    painter.setFont(font);
    painter.setPen(Qt::white);
    painter.drawText(QRectF(0, 0, kSize, kSize), Qt::AlignCenter, label);

    return QIcon(pixmap);
}

QIcon sendIcon(const QColor& fillColor) {
    constexpr int kSize = 24;
    constexpr qreal kDevicePixelRatio = 2.0;

    QPixmap pixmap(QSize(kSize, kSize) * kDevicePixelRatio);
    pixmap.setDevicePixelRatio(kDevicePixelRatio);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(fillColor);

    // Стрелка "бумажный самолётик", направленная вправо, с вырезом у
    // хвоста — узнаваемый глиф "отправить" без необходимости в SVG.
    QPainterPath path;
    path.moveTo(4, 20);
    path.lineTo(21, 12);
    path.lineTo(4, 4);
    path.lineTo(9, 12);
    path.closeSubpath();
    painter.drawPath(path);

    return QIcon(pixmap);
}

}  // namespace devicehub::ui_icons
