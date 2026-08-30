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
 * @brief Issues and verifies HMAC-SHA256-signed, time-limited tokens —
 *        both short-lived access tokens and long-lived refresh tokens
 *        (issue #105).
 *
 * Token shape: base64url(json payload) + "." + base64url(HMAC-SHA256 of
 * the payload segment). Deliberately not full JWT (no header segment, no
 * algorithm negotiation) — this service only ever produces and consumes
 * its own tokens, so the extra JWT machinery buys nothing.
 *
 * A refresh token is the same shape plus `"typ": "refresh"` in the
 * payload; an access token's payload never has that key. verifyToken()
 * requires that key be absent, verifyRefreshToken() requires it be
 * present, so a refresh token can never be used in place of an access
 * token (or vice versa) even though both are signed with the same
 * secret. Refresh tokens don't rotate — redeeming one via
 * verifyRefreshToken() + issueToken() just mints a fresh access token,
 * the same refresh token keeps working until it expires.
 *
 * Pure logic, no networking: testable in isolation from HttpServer.
 */
class TokenService {
public:
    explicit TokenService(std::string secret, std::chrono::seconds ttl = std::chrono::seconds{3600},
                           std::chrono::seconds refreshTtl = std::chrono::seconds{30 * 24 * 3600});

    /// Issues a new access token for @p subject, valid for this
    /// service's (short) TTL.
    [[nodiscard]] Token issueToken(const std::string& subject) const;

    /// @return The access token's subject if @p token has a valid
    ///         signature, hasn't expired, and isn't actually a refresh
    ///         token; std::nullopt otherwise.
    [[nodiscard]] std::optional<std::string> verifyToken(const std::string& token) const;

    /// Issues a new refresh token for @p subject, valid for this
    /// service's (long) refresh TTL — exchanged via verifyRefreshToken()
    /// for fresh access tokens without the user re-entering credentials.
    [[nodiscard]] Token issueRefreshToken(const std::string& subject) const;

    /// @return @p token's subject if it's a valid, unexpired refresh
    ///         token; std::nullopt otherwise — including when given a
    ///         plain access token, which is rejected here rather than
    ///         silently accepted.
    [[nodiscard]] std::optional<std::string> verifyRefreshToken(const std::string& token) const;

private:
    [[nodiscard]] Token issueTokenInternal(const std::string& subject, std::chrono::seconds ttl,
                                            bool isRefresh) const;
    [[nodiscard]] std::optional<std::string> verifyTokenInternal(const std::string& token, bool expectRefresh) const;

    std::string secret_;
    std::chrono::seconds ttl_;
    std::chrono::seconds refreshTtl_;
};

}  // namespace auth_service
