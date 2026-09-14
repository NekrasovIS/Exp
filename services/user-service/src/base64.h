#pragma once

#include <optional>
#include <string>

namespace user_service::base64 {

/// Кодирует @p bytes в стандартный (RFC 4648 §4, алфавит '+'/'/', с
/// '='-паддингом) base64-текст — issue #388: TOTP-секрет хранится как
/// base64 TEXT в user_totp, тем же приёмом ("base64-as-TEXT сайдстепит
/// binary-параметры libpqxx"), что и вложения chat-service (issue #116,
/// см. doc-комментарий над `attachments` в его db/init.sql) и аватары
/// (issue #384) этого же сервиса.
[[nodiscard]] std::string encode(const std::string& bytes);

/// Декодирует стандартный (RFC 4648 §4, алфавит '+'/'/', с '='-паддингом)
/// base64-текст — формат, который QByteArray::toBase64() выдаёт на
/// стороне DeviceHub (payload загрузки вложений из issue #116). Паддинг
/// '=' и пробелы/переводы строк пропускаются, а не считаются недопустимыми.
/// @return Декодированные байты, либо std::nullopt, если @p text содержит
///         символ вне алфавита base64 (и это не паддинг и не пробел).
[[nodiscard]] std::optional<std::string> decode(const std::string& text);

}  // namespace user_service::base64
