#include "ui/QrCodeGenerator.h"

#include <QPainter>

#include <qrencode.h>

namespace devicehub::qr_code_generator {

QImage encode(const QString& text, int moduleSize) {
    // version=0 — libqrencode сама выбирает наименьшую версию (размер
    // сетки), в которую помещается text; QR_ECLEVEL_M — тот же уровень
    // коррекции ошибок, что обычно используют приложения-
    // аутентификаторы для otpauth://-URI (баланс ёмкости против
    // устойчивости к загрязнению камеры/экрана).
    QRcode* qrCode = QRcode_encodeString(text.toUtf8().constData(), 0, QR_ECLEVEL_M, QR_MODE_8, 1);
    if (qrCode == nullptr) {
        return {};
    }

    // Один модуль QR — квадрат moduleSize×moduleSize пикселей, а не
    // 1:1, иначе итоговое изображение (обычно 20-40 модулей в ширину)
    // получилось бы слишком мелким, чтобы его отсканировала камера
    // телефона без масштабирования.
    const int pixelWidth = qrCode->width * moduleSize;
    QImage image(pixelWidth, pixelWidth, QImage::Format_RGB32);
    image.fill(Qt::white);

    // QImage само по себе не рисует фигуры (fillRect есть только у
    // QPainter/QPixmap) — рисуем прямо в image через QPainter, как и
    // полагается для растеризации примитивов на QImage.
    QPainter painter(&image);
    for (int y = 0; y < qrCode->width; ++y) {
        for (int x = 0; x < qrCode->width; ++x) {
            // Младший бит каждого байта data — 1 для чёрного модуля,
            // остальные биты — служебная информация libqrencode,
            // ненужная здесь (см. qrencode.h: "band 0 is less
            // significant bit").
            const bool isDark = (qrCode->data[(y * qrCode->width) + x] & 1) != 0;
            if (isDark) {
                painter.fillRect(x * moduleSize, y * moduleSize, moduleSize, moduleSize, Qt::black);
            }
        }
    }
    painter.end();

    QRcode_free(qrCode);
    return image;
}

}  // namespace devicehub::qr_code_generator
