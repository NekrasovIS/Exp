#include "ui/QrCodeGenerator.h"

#include <gtest/gtest.h>

namespace devicehub {
namespace {

// Только smoke-тесты, тот же принцип, что и у IconFactoryTest — QR-код
// рисуется вручную через QPainter, единственное, что можно осмысленно
// проверить без сэмплирования пикселей/декодирования обратно, это что
// возвращается непустое изображение разумного размера.

TEST(QrCodeGeneratorTest, EncodeReturnsANonNullSquareImage) {
    const QImage image = qr_code_generator::encode(
        QStringLiteral("otpauth://totp/DeviceHub:alice?secret=JBSWY3DPEHPK3PXP&issuer=DeviceHub"));

    ASSERT_FALSE(image.isNull());
    EXPECT_EQ(image.width(), image.height());
    EXPECT_GT(image.width(), 0);
}

TEST(QrCodeGeneratorTest, ModuleSizeScalesTheOutputProportionally) {
    const QString text = QStringLiteral("otpauth://totp/DeviceHub:bob?secret=ABCDEFGHIJKLMNOP");

    const QImage small = qr_code_generator::encode(text, 2);
    const QImage large = qr_code_generator::encode(text, 8);

    ASSERT_FALSE(small.isNull());
    ASSERT_FALSE(large.isNull());
    // Оба кодируют один и тот же текст, значит используют одну и ту же
    // версию/сетку QR — отношение размеров должно точно совпадать с
    // отношением moduleSize.
    EXPECT_EQ(large.width(), small.width() * 4);
}

TEST(QrCodeGeneratorTest, DifferentTextProducesDifferentImages) {
    const QImage first = qr_code_generator::encode(QStringLiteral("otpauth://totp/DeviceHub:alice?secret=AAAA"));
    const QImage second = qr_code_generator::encode(QStringLiteral("otpauth://totp/DeviceHub:bob?secret=BBBB"));

    ASSERT_FALSE(first.isNull());
    ASSERT_FALSE(second.isNull());
    EXPECT_NE(first, second);
}

}  // namespace
}  // namespace devicehub
