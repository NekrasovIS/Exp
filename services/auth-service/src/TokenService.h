#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>

namespace auth_service {

/// A freshly issued token and when it expires (Unix seconds, UTC).
struct Token {
    std::string value;
    std::int64_t expiresAt = 0;
};

/**
 * @brief Issues and verifies HMAC-SHA256-signed, time-limited tokens.
 *
 * Token shape: base64url(json payload) + "." + base64url(HMAC-SHA256 of
 * the payload segment). Deliberately not full JWT (no header segment, no
 * algorithm negotiation) — this service only ever produces and consumes
 * its own tokens, so the extra JWT machinery buys nothing.
 *
 * Pure logic, no networking: testable in isolation from HttpServer.
 */
class TokenService {
public:
    explicit TokenService(std::string secret, std::chrono::seconds ttl = std::chrono::seconds{3600});

    /// Issues a new token for @p subject, valid for this service's TTL.
    [[nodiscard]] Token issueToken(const std::string& subject) const;

    /// @return The token's subject if @p token has a valid signature and
    ///         hasn't expired; std::nullopt otherwise.
    [[nodiscard]] std::optional<std::string> verifyToken(const std::string& token) const;

private:
    std::string secret_;
    std::chrono::seconds ttl_;
};

}  // namespace auth_service
