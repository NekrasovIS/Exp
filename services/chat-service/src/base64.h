#pragma once

#include <optional>
#include <string>

namespace chat_service::base64 {

/// Декодирует стандартный (RFC 4648 §4, алфавит '+'/'/', с '='-паддингом)
/// base64-текст — формат, который QByteArray::toBase64() выдаёт на
/// стороне DeviceHub (payload загрузки вложений из issue #116). Паддинг
/// '=' и пробелы/переводы строк пропускаются, а не считаются недопустимыми.
/// @return Декодированные байты, либо std::nullopt, если @p text содержит
///         символ вне алфавита base64 (и это не паддинг и не пробел).
[[nodiscard]] std::optional<std::string> decode(const std::string& text);

}  // namespace chat_service::base64
