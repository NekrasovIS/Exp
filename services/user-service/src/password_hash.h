#pragma once

#include <string>

namespace password_hash {

/// Хеширует @p password с помощью Argon2id (crypto_pwhash_str из libsodium) —
/// соль и параметры встроены в возвращаемую строку, ничего дополнительного
/// хранить рядом с ней не требуется.
[[nodiscard]] std::string hash(const std::string& password);

/// @return True, если @p password соответствует ранее вычисленному хешу @p hash.
[[nodiscard]] bool verify(const std::string& hash, const std::string& password);

}  // namespace password_hash
