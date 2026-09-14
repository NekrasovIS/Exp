#include "totp.h"

#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <sodium.h>

#include <array>
#include <cstdio>

namespace user_service::totp {

namespace {
constexpr int kTimeStepSeconds = 30;

/// RFC 4226 §5.3 dynamic truncation — extracts a 31-bit integer from
/// @p digest at a position determined by its own last nibble, so an
/// attacker can't predict which four bytes carry the OTP without
/// already having the digest.
uint32_t dynamicTruncate(const unsigned char* digest, unsigned int length) {
    const unsigned char offset = digest[length - 1] & 0x0f;
    return ((static_cast<uint32_t>(digest[offset]) & 0x7f) << 24) |
           ((static_cast<uint32_t>(digest[offset + 1]) & 0xff) << 16) |
           ((static_cast<uint32_t>(digest[offset + 2]) & 0xff) << 8) |
           (static_cast<uint32_t>(digest[offset + 3]) & 0xff);
}

uint32_t pow10(int exponent) {
    uint32_t result = 1;
    for (int i = 0; i < exponent; ++i) {
        result *= 10;
    }
    return result;
}
}  // namespace

std::string hotp(const std::string& key, uint64_t counter, int digits) {
    // Big-endian 8-byte counter — RFC 4226 §5.2 requires it, not the
    // host's native byte order.
    std::array<unsigned char, 8> counterBytes{};
    for (int i = 7; i >= 0; --i) {
        counterBytes[static_cast<std::size_t>(i)] = static_cast<unsigned char>(counter & 0xff);
        counter >>= 8;
    }

    std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
    unsigned int digestLength = 0;
    HMAC(EVP_sha1(), key.data(), static_cast<int>(key.size()), counterBytes.data(), counterBytes.size(),
         digest.data(), &digestLength);

    const uint32_t truncated = dynamicTruncate(digest.data(), digestLength) % pow10(digits);

    // Zero-padded to exactly `digits` characters — snprintf, not
    // std::to_string, because a code like "007081" must not lose its
    // leading zeros.
    std::array<char, 16> buffer{};
    std::snprintf(buffer.data(), buffer.size(), "%0*u", digits, truncated);
    return std::string(buffer.data());
}

std::string totp(const std::string& key, int64_t unixTimeSeconds, int digits) {
    const auto counter = static_cast<uint64_t>(unixTimeSeconds / kTimeStepSeconds);
    return hotp(key, counter, digits);
}

bool verify(const std::string& key, const std::string& code, int64_t unixTimeSeconds, int windowSteps,
            int digits) {
    for (int step = -windowSteps; step <= windowSteps; ++step) {
        const int64_t stepTime = unixTimeSeconds + static_cast<int64_t>(step) * kTimeStepSeconds;
        if (totp(key, stepTime, digits) == code) {
            return true;
        }
    }
    return false;
}

std::string generateSecret(std::size_t byteLength) {
    std::string secret(byteLength, '\0');
    randombytes_buf(secret.data(), byteLength);
    return secret;
}

}  // namespace user_service::totp
