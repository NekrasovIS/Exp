#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace base64_utils {

/// Encodes @p data as unpadded base64url (RFC 4648 §5), as used in the
/// token format: '+' -> '-', '/' -> '_', no trailing '='.
[[nodiscard]] std::string encodeUrl(const std::vector<uint8_t>& data);

/// Decodes unpadded base64url text produced by encodeUrl().
/// @return The decoded bytes, or an empty vector if @p text is malformed.
[[nodiscard]] std::vector<uint8_t> decodeUrl(const std::string& text);

}  // namespace base64_utils
