#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace user_service::totp {

/// HOTP (RFC 4226) value for @p key at @p counter, truncated to
/// @p digits decimal digits (zero-padded on the left). @p key is the
/// raw secret bytes, not base32-encoded — see base32.h for that
/// separate, display-only encoding.
[[nodiscard]] std::string hotp(const std::string& key, uint64_t counter, int digits = 6);

/// TOTP (RFC 6238) value for @p key at @p unixTimeSeconds, using the
/// standard 30-second time step (`floor(unixTimeSeconds / 30)` as the
/// HOTP counter).
[[nodiscard]] std::string totp(const std::string& key, int64_t unixTimeSeconds, int digits = 6);

/// True if @p code matches the TOTP for @p key at @p unixTimeSeconds,
/// or at up to @p windowSteps time-steps before/after it — issue #388:
/// authenticator apps and this server's clocks aren't perfectly
/// synchronized, and a code entered right at a 30-second boundary may
/// already belong to the next/previous step by the time this runs.
[[nodiscard]] bool verify(const std::string& key, const std::string& code, int64_t unixTimeSeconds,
                          int windowSteps = 1, int digits = 6);

/// Cryptographically random secret, @p byteLength raw bytes — default
/// 20 bytes (160 bits) is RFC 4226's recommended minimum key length for
/// HMAC-SHA1, and what most authenticator apps assume.
[[nodiscard]] std::string generateSecret(std::size_t byteLength = 20);

}  // namespace user_service::totp
