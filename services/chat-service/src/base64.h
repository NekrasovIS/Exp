#pragma once

#include <optional>
#include <string>

namespace chat_service::base64 {

/// Decodes standard (RFC 4648 §4, '+'/'/' alphabet, '='-padded) base64
/// text — the format QByteArray::toBase64() produces on the DeviceHub
/// side (issue #116's attachment upload payload). '=' padding and
/// whitespace/newlines are skipped rather than treated as invalid.
/// @return The decoded bytes, or std::nullopt if @p text contains a
///         character outside the base64 alphabet (and isn't padding
///         or whitespace).
[[nodiscard]] std::optional<std::string> decode(const std::string& text);

}  // namespace chat_service::base64
