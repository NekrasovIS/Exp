#pragma once

#include <string>

namespace password_hash {

/// Hashes @p password with Argon2id (libsodium's crypto_pwhash_str) —
/// salt and parameters are embedded in the returned string, nothing
/// extra needs to be stored alongside it.
[[nodiscard]] std::string hash(const std::string& password);

/// @return True if @p password matches the previously hashed @p hash.
[[nodiscard]] bool verify(const std::string& hash, const std::string& password);

}  // namespace password_hash
