#pragma once

#include <optional>
#include <string>

namespace user_service::base64 {

/// Декодирует стандартный (RFC 4648 §4, алфавит '+'/'/', с '='-паддингом)
/// base64-текст — та же реализация, что и chat_service::base64::decode()
/// (issue #116/#384): каждый сервис в этом репозитории независим и не
/// шарит код с другими (см. CLAUDE.md), а формат ровно тот же, что уже
/// выдаёт `QByteArray::toBase64()`/браузерный `FileReader`. Паддинг '='
/// и пробелы/переводы строк пропускаются, а не считаются недопустимыми.
/// @return Декодированные байты, либо std::nullopt, если @p text содержит
///         символ вне алфавита base64 (и это не паддинг и не пробел).
[[nodiscard]] std::optional<std::string> decode(const std::string& text);

}  // namespace user_service::base64
