#pragma once

#include <optional>
#include <string>

namespace chat_service {

/**
 * @brief Calls auth-service's POST /auth/verify to resolve a token.
 *
 * Fails closed: any network/protocol error or an invalid/expired token
 * is treated as "not authenticated" rather than propagating an exception.
 */
class AuthServiceClient {
public:
    AuthServiceClient(std::string host, int port);

    /// @return The token's subject (login) if it's valid, std::nullopt otherwise.
    [[nodiscard]] std::optional<std::string> verifyToken(const std::string& token) const;

private:
    std::string host_;
    int port_;
};

}  // namespace chat_service
