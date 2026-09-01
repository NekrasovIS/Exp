#pragma once

#include <optional>
#include <string>
#include <utility>

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

    /// @return @p login's profile, or std::nullopt if no such user (issue #110).
    [[nodiscard]] std::optional<Profile> getProfile(const std::string& login);

    /// Перезаписывает display_name/avatar_url/public_key/email для @p login.
    [[nodiscard]] UpdateProfileResult updateProfile(const std::string& login, const ProfileUpdate& update);

    /// См. UserRepository::resolveOtpIdentifier() (issue #156).
    [[nodiscard]] std::optional<std::pair<std::string, std::string>> resolveOtpIdentifier(
        const std::string& identifier);

private:
    UserRepository& repository_;
};

}  // namespace user_service
