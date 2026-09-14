#include "ui/IconFactory.h"

#include <QBuffer>
#include <QByteArray>
#include <QColor>
#include <QFont>
#include <QIODevice>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPixmap>
#include <QString>

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

QIcon communityAvatarIcon(const QString& label, qint64 unreadCount) {
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
    // Круг, а не скруглённый квадрат — аватар сообщества (issue #182).
    painter.drawEllipse(QRectF(0, 0, kSize, kSize));

    QFont font = painter.font();
    font.setBold(true);
    font.setPointSizeF(14.0);
    painter.setFont(font);
    painter.setPen(Qt::white);
    painter.drawText(QRectF(0, 0, kSize, kSize), Qt::AlignCenter, label);

    if (unreadCount > 0) {
        // Issue #310/#349: сумма непрочитанных по каналам сообщества —
        // маленький красный круг в правом верхнем углу, поверх основного
        // круга-аватара, с белой обводкой, чтобы отделиться от него на
        // любом фоне. "99+" вместо трёхзначных чисел — тот же приём,
        // что и у мессенджеров, чтобы бейдж не разрастался бесконечно.
        constexpr qreal kBadgeDiameter = kSize * 0.46;
        const QRectF badgeRect(kSize - kBadgeDiameter * 0.8, -kBadgeDiameter * 0.2, kBadgeDiameter, kBadgeDiameter);
        painter.setPen(QPen(Qt::white, 1.5));
        painter.setBrush(QColor(devicehub::ui_theme::kBadgeBackground));
        painter.drawEllipse(badgeRect);

        QFont badgeFont = painter.font();
        badgeFont.setPointSizeF(unreadCount > 99 ? 7.5 : 9.0);
        painter.setFont(badgeFont);
        painter.setPen(Qt::white);
        painter.drawText(badgeRect, Qt::AlignCenter, unreadCount > 99 ? QStringLiteral("99+")
                                                                       : QString::number(unreadCount));
    }

    return QIcon(pixmap);
}

QIcon memberAvatarIcon(const QString& label, bool online) {
    constexpr int kSize = 40;
    constexpr qreal kDevicePixelRatio = 2.0;
    // Border-in-panel-background trick makes the dot read as "cut out
    // of" the avatar rather than just stacked on top of it — same
    // visual language most chat apps use for a presence dot.
    constexpr qreal kDotDiameter = 13;
    constexpr qreal kDotBorder = 2.5;

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
    painter.drawEllipse(QRectF(0, 0, kSize, kSize));

    QFont font = painter.font();
    font.setBold(true);
    font.setPointSizeF(14.0);
    painter.setFont(font);
    painter.setPen(Qt::white);
    painter.drawText(QRectF(0, 0, kSize, kSize), Qt::AlignCenter, label);

    if (online) {
        const QRectF dotRect(kSize - kDotDiameter - 1, kSize - kDotDiameter - 1, kDotDiameter, kDotDiameter);
        painter.setPen(QPen(QColor(devicehub::ui_theme::kSidebarBackground), kDotBorder));
        painter.setBrush(QColor(devicehub::ui_theme::kAccentGradientStart));
        painter.drawEllipse(dotRect);
    }

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

QIcon settingsIcon(const QColor& fillColor) {
    constexpr int kSize = 24;
    constexpr qreal kDevicePixelRatio = 2.0;
    constexpr int kToothCount = 8;
    constexpr qreal kOuterRadius = 7.5;
    constexpr qreal kInnerRadius = 3.4;
    constexpr qreal kToothWidth = 3.0;
    constexpr qreal kToothLength = 3.4;

    QPixmap pixmap(QSize(kSize, kSize) * kDevicePixelRatio);
    pixmap.setDevicePixelRatio(kDevicePixelRatio);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.translate(kSize / 2.0, kSize / 2.0);
    painter.setPen(Qt::NoPen);
    painter.setBrush(fillColor);

    // Зубцы — kToothCount небольших скруглённых прямоугольников,
    // повёрнутых равномерно вокруг центра.
    for (int i = 0; i < kToothCount; ++i) {
        painter.save();
        painter.rotate(360.0 / kToothCount * i);
        painter.drawRoundedRect(
            QRectF(-kToothWidth / 2.0, -kOuterRadius - kToothLength + 1.0, kToothWidth, kToothLength), 1.0, 1.0);
        painter.restore();
    }

    // Кольцо тела шестерёнки с полой серединой — одна заливка с
    // правилом odd-even вместо двух отдельных фигур.
    QPainterPath ring;
    ring.addEllipse(QPointF(0, 0), kOuterRadius, kOuterRadius);
    ring.addEllipse(QPointF(0, 0), kInnerRadius, kInnerRadius);
    ring.setFillRule(Qt::OddEvenFill);
    painter.drawPath(ring);

    return QIcon(pixmap);
}

QIcon membersIcon(const QColor& fillColor) {
    constexpr int kSize = 24;
    constexpr qreal kDevicePixelRatio = 2.0;
    constexpr qreal kHeadRadius = 2.6;
    constexpr qreal kHeadOffsetX = 5.0;
    constexpr qreal kHeadY = -4.0;
    constexpr qreal kBodyHalfWidth = 3.0;
    constexpr qreal kBodyTop = -1.0;
    constexpr qreal kBodyBottom = 7.0;
    constexpr qreal kBodyRadius = 2.0;

    QPixmap pixmap(QSize(kSize, kSize) * kDevicePixelRatio);
    pixmap.setDevicePixelRatio(kDevicePixelRatio);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.translate(kSize / 2.0, kSize / 2.0);
    painter.setPen(Qt::NoPen);
    painter.setBrush(fillColor);

    // Два силуэта человека (голова + плечи) бок о бок с зазором между
    // ними — не перекрываются, иначе на сплошной заливке без обводки
    // они слились бы в одно пятно без видимого шва между силуэтами.
    for (const qreal centerX : {-kHeadOffsetX, kHeadOffsetX}) {
        painter.drawEllipse(QPointF(centerX, kHeadY), kHeadRadius, kHeadRadius);
        painter.drawRoundedRect(
            QRectF(centerX - kBodyHalfWidth, kBodyTop, kBodyHalfWidth * 2.0, kBodyBottom - kBodyTop), kBodyRadius,
            kBodyRadius);
    }

    return QIcon(pixmap);
}

QIcon micIcon(const QColor& fillColor) {
    constexpr int kSize = 24;
    constexpr qreal kDevicePixelRatio = 2.0;
    constexpr qreal kHeadWidth = 7.0;
    constexpr qreal kHeadHeight = 11.0;
    constexpr qreal kHeadTop = 2.5;

    QPixmap pixmap(QSize(kSize, kSize) * kDevicePixelRatio);
    pixmap.setDevicePixelRatio(kDevicePixelRatio);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.translate(kSize / 2.0, 0.0);
    painter.setPen(Qt::NoPen);
    painter.setBrush(fillColor);

    // Капсула — головка микрофона.
    painter.drawRoundedRect(QRectF(-kHeadWidth / 2.0, kHeadTop, kHeadWidth, kHeadHeight), kHeadWidth / 2.0,
                             kHeadWidth / 2.0);

    // Дужка-подставка (нижняя половина окружности, контуром — иначе
    // слилась бы со сплошной заливкой головки) и ножка со стойкой.
    painter.setPen(QPen(fillColor, 1.6, Qt::SolidLine, Qt::RoundCap));
    painter.setBrush(Qt::NoBrush);
    painter.drawArc(QRectF(-5.5, kHeadTop + kHeadHeight - 4.5, 11.0, 9.0), 0, -180 * 16);
    painter.drawLine(QPointF(0, kHeadTop + kHeadHeight), QPointF(0, 21));
    painter.drawLine(QPointF(-3.5, 21), QPointF(3.5, 21));

    return QIcon(pixmap);
}

QIcon stopIcon(const QColor& fillColor) {
    constexpr int kSize = 24;
    constexpr qreal kDevicePixelRatio = 2.0;
    constexpr qreal kSquareSize = 10.0;

    QPixmap pixmap(QSize(kSize, kSize) * kDevicePixelRatio);
    pixmap.setDevicePixelRatio(kDevicePixelRatio);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(fillColor);
    painter.drawRoundedRect(
        QRectF((kSize - kSquareSize) / 2.0, (kSize - kSquareSize) / 2.0, kSquareSize, kSquareSize), 2.0, 2.0);

    return QIcon(pixmap);
}

QIcon pinIcon(const QColor& fillColor) {
    constexpr int kSize = 24;
    constexpr qreal kDevicePixelRatio = 2.0;

    QPixmap pixmap(QSize(kSize, kSize) * kDevicePixelRatio);
    pixmap.setDevicePixelRatio(kDevicePixelRatio);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(fillColor);

    // Круглая головка канцелярской кнопки сверху и остриё-треугольник
    // снизу — тот же узнаваемый силуэт "pin", что и у большинства
    // мессенджеров.
    painter.drawEllipse(QPointF(12, 8), 5.5, 5.5);
    QPainterPath needle;
    needle.moveTo(8.5, 11.5);
    needle.lineTo(15.5, 11.5);
    needle.lineTo(12, 21);
    needle.closeSubpath();
    painter.drawPath(needle);

    return QIcon(pixmap);
}

QIcon playIcon(const QColor& fillColor) {
    constexpr int kSize = 24;
    constexpr qreal kDevicePixelRatio = 2.0;

    QPixmap pixmap(QSize(kSize, kSize) * kDevicePixelRatio);
    pixmap.setDevicePixelRatio(kDevicePixelRatio);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(fillColor);

    QPainterPath path;
    path.moveTo(7, 4.5);
    path.lineTo(19, 12);
    path.lineTo(7, 19.5);
    path.closeSubpath();
    painter.drawPath(path);

    return QIcon(pixmap);
}

QIcon pauseIcon(const QColor& fillColor) {
    constexpr int kSize = 24;
    constexpr qreal kDevicePixelRatio = 2.0;
    constexpr qreal kBarWidth = 4.0;
    constexpr qreal kBarHeight = 14.0;
    constexpr qreal kGap = 3.0;

    QPixmap pixmap(QSize(kSize, kSize) * kDevicePixelRatio);
    pixmap.setDevicePixelRatio(kDevicePixelRatio);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(fillColor);

    const qreal top = (kSize - kBarHeight) / 2.0;
    painter.drawRoundedRect(QRectF(kSize / 2.0 - kGap / 2.0 - kBarWidth, top, kBarWidth, kBarHeight), 1.5, 1.5);
    painter.drawRoundedRect(QRectF(kSize / 2.0 + kGap / 2.0, top, kBarWidth, kBarHeight), 1.5, 1.5);

    return QIcon(pixmap);
}

QIcon videoIcon(const QColor& fillColor) {
    constexpr int kSize = 24;
    constexpr qreal kDevicePixelRatio = 2.0;

    QPixmap pixmap(QSize(kSize, kSize) * kDevicePixelRatio);
    pixmap.setDevicePixelRatio(kDevicePixelRatio);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(fillColor);

    // Корпус камеры (скруглённый прямоугольник) и объектив-трапеция,
    // выступающий справа, — классический силуэт видеокамеры.
    painter.drawRoundedRect(QRectF(3, 6, 12, 12), 2.0, 2.0);
    QPainterPath lens;
    lens.moveTo(15, 10);
    lens.lineTo(21, 7);
    lens.lineTo(21, 17);
    lens.lineTo(15, 14);
    lens.closeSubpath();
    painter.drawPath(lens);

    return QIcon(pixmap);
}

QString iconHtml(const QIcon& icon, int sizePx) {
    QByteArray pngBytes;
    QBuffer buffer(&pngBytes);
    buffer.open(QIODevice::WriteOnly);
    icon.pixmap(sizePx, sizePx).save(&buffer, "PNG");
    return QStringLiteral("<img src=\"data:image/png;base64,%1\" width=\"%2\" height=\"%2\">")
        .arg(QString::fromLatin1(pngBytes.toBase64()))
        .arg(sizePx);
}

}  // namespace devicehub::ui_icons
