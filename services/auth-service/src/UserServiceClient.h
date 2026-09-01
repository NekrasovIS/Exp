#pragma once

#include <string>

namespace auth_service {

/**
 * @brief Вызывает POST /users/verify-credentials и POST /users/register
 *        у user-service.
 *
 * Отказывает по умолчанию (fails closed): любая сетевая/протокольная
 * ошибка трактуется как "не подтверждено"/"не зарегистрировано", а не
 * распространяется исключением в обработчик запроса.
 */
class UserServiceClient {
public:
    UserServiceClient(std::string host, int port);

    [[nodiscard]] bool verifyCredentials(const std::string& login, const std::string& password) const;

    /// Вызывает POST /users/register у user-service.
    /// @return True, если логин был свободен и аккаунт был создан.
    [[nodiscard]] bool registerUser(const std::string& login, const std::string& password) const;

private:
    std::string host_;
    int port_;
};

}  // namespace auth_service
