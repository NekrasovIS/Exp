#pragma once

#include <optional>
#include <string>

#include "UserRepository.h"

namespace user_service {

/**
 * @brief Бизнес-логика регистрации и входа: хеширует/проверяет пароли
 *        (password_hash) и делегирует хранение UserRepository.
 */
class UserService {
public:
    explicit UserService(UserRepository& repository);

    /// @return True, если @p login был свободен и учётная запись создана.
    [[nodiscard]] bool registerUser(const std::string& login, const std::string& password);

    /// @return True, если @p login существует и @p password соответствует его хешу.
    [[nodiscard]] bool verifyCredentials(const std::string& login, const std::string& password);

    /// @return Профиль @p login, или std::nullopt, если такого пользователя нет (issue #110).
    [[nodiscard]] std::optional<Profile> getProfile(const std::string& login);

    /// Перезаписывает display_name/avatar_url/public_key/email для @p login.
    [[nodiscard]] UpdateProfileResult updateProfile(const std::string& login, const ProfileUpdate& update);

    /// См. UserRepository::resolveOtpIdentifier() (issue #156/#174).
    [[nodiscard]] std::optional<OtpIdentity> resolveOtpIdentifier(const std::string& identifier);

private:
    UserRepository& repository_;
};

}  // namespace user_service
