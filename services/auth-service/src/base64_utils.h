#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace base64_utils {

/// Кодирует @p data в base64url без дополнения (RFC 4648 §5), как это
/// используется в формате токена: '+' -> '-', '/' -> '_', без завершающего '='.
[[nodiscard]] std::string encodeUrl(const std::vector<uint8_t>& data);

/// Декодирует base64url-текст без дополнения, произведённый encodeUrl().
/// @return Декодированные байты, либо пустой вектор, если @p text некорректен.
[[nodiscard]] std::vector<uint8_t> decodeUrl(const std::string& text);

}  // namespace base64_utils
