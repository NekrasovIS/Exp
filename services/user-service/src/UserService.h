#pragma once

#include <string>

#include "UserRepository.h"

namespace user_service {

/**
 * @brief Business logic for registration and login: hashes/verifies
 *        passwords (password_hash) and delegates storage to UserRepository.
 */
class UserService {
public:
    explicit UserService(UserRepository& repository);

    /// @return True if @p login was free and the account was created.
    [[nodiscard]] bool registerUser(const std::string& login, const std::string& password);

    /// @return True if @p login exists and @p password matches its hash.
    [[nodiscard]] bool verifyCredentials(const std::string& login, const std::string& password);

private:
    UserRepository& repository_;
};

}  // namespace user_service
