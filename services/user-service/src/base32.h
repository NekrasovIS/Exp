#pragma once

#include <optional>
#include <string>

namespace user_service::base32 {

/// Кодирует @p bytes в стандартный (RFC 4648 §6, алфавит A-Z2-7,
/// с '='-паддингом) base32-текст — issue #388: TOTP-секрет
/// представляется в `otpauth://` URI и для ручного ввода в этом
/// формате, а не base64, потому что base32's алфавит без цифр 0/1/8/9,
/// визуально неотличимых от букв, и без регистрозависимости — то, что
/// реально печатают/вводят вручную приложения-аутентификаторы.
[[nodiscard]] std::string encode(const std::string& bytes);

/// Декодирует то, что вернул encode() — сам сервер это никогда не
/// делает (секрет хранится как base64, см. UserService::setupTotp()),
/// нужно только для тестов, проверяющих код TOTP от секрета, который
/// клиент получил бы в base32-виде.
/// @return std::nullopt, если @p text содержит символ вне алфавита
///         (и это не паддинг/пробел).
[[nodiscard]] std::optional<std::string> decode(const std::string& text);

}  // namespace user_service::base32
