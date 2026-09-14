#pragma once

#include <optional>
#include <string>

namespace user_service::base64 {

/// Декодирует стандартный (RFC 4648 §4, алфавит '+'/'/', с '='-паддингом)
/// base64-текст — тот же формат, что уже декодирует chat-service для
/// вложений (issue #384 — эндпоинт загрузки аватара сделан по её
/// образцу). Паддинг '=' и пробелы/переводы строк пропускаются, а не
/// считаются недопустимыми.
/// @return Декодированные байты, либо std::nullopt, если @p text содержит
///         символ вне алфавита base64 (и это не паддинг и не пробел).
[[nodiscard]] std::optional<std::string> decode(const std::string& text);

}  // namespace user_service::base64
