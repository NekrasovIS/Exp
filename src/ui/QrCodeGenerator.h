#pragma once

#include <QImage>
#include <QString>

namespace devicehub::qr_code_generator {

/// Кодирует @p text (для TotpSetupDialog, issue #390 — это
/// otpauth://-URI) в чёрно-белый QR-код через libqrencode, каждый
/// модуль — квадрат @p moduleSize пикселей (по умолчанию с запасом на
/// сканирование телефоном без зума). Версия/уровень коррекции ошибок
/// выбираются автоматически под длину @p text — вызывающему коду не
/// нужно ничего знать о формате QR, только сам конечный QImage.
/// @return пустой (null) QImage, если @p text не удалось закодировать
///         (libqrencode вернула ошибку) — вызывающий код должен
///         проверить isNull() перед использованием.
[[nodiscard]] QImage encode(const QString& text, int moduleSize = 6);

}  // namespace devicehub::qr_code_generator
