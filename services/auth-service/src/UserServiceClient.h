#pragma once

#include <string>

namespace auth_service {

/**
 * @brief Calls user-service's POST /users/verify-credentials and
 *        POST /users/register.
 *
 * Fails closed: any network/protocol error is treated as "not
 * verified"/"not registered" rather than propagating an exception into
 * the request handler.
 */
class UserServiceClient {
public:
    UserServiceClient(std::string host, int port);

    [[nodiscard]] bool verifyCredentials(const std::string& login, const std::string& password) const;

    /// Calls user-service's POST /users/register.
    /// @return True if the login was free and the account was created.
    [[nodiscard]] bool registerUser(const std::string& login, const std::string& password) const;

private:
    std::string host_;
    int port_;
};

}  // namespace auth_service
