#pragma once

#include <optional>
#include <string>
#include <utility>

namespace auth_service {

/**
 * @brief Вызывает user-service: POST /users/verify-credentials,
 *        POST /users/register и POST /users/resolve-otp-identifier
 *        (issue #156).
 *
 * Fail closed: любая сетевая/протокольная ошибка трактуется как "не
 * подтверждено"/"не зарегистрировано"/"не найдено", а не пробрасывает
 * исключение в обработчик запроса.
 */
class UserServiceClient {
public:
    UserServiceClient(std::string host, int port);

    [[nodiscard]] bool verifyCredentials(const std::string& login, const std::string& password) const;

    /// Вызывает POST /users/register.
    /// @return true, если login был свободен и аккаунт создан.
    [[nodiscard]] bool registerUser(const std::string& login, const std::string& password) const;

    /// Вызывает POST /users/resolve-otp-identifier — приводит
    /// @p identifier (login или email) к паре (login, email) для
    /// входа по одноразовому коду (issue #156). @return std::nullopt,
    /// если такого пользователя нет или у него не задан email.
    [[nodiscard]] std::optional<std::pair<std::string, std::string>> resolveOtpIdentifier(
        const std::string& identifier) const;

private:
    std::string host_;
    int port_;
};

}  // namespace auth_service
