#pragma once

#include <optional>
#include <string>

namespace user_service {

/**
 * @brief Вызывает POST /auth/verify у auth-service для проверки токена.
 *
 * Отказывает по умолчанию (fail closed): любая сетевая/протокольная ошибка
 * или недействительный/истёкший токен трактуются как "не аутентифицирован",
 * а не приводят к выбросу исключения.
 */
class AuthServiceClient {
public:
    AuthServiceClient(std::string host, int port);

    /// @return Субъект токена (логин), если токен действителен, иначе std::nullopt.
    [[nodiscard]] std::optional<std::string> verifyToken(const std::string& token) const;

private:
    std::string host_;
    int port_;
};

}  // namespace user_service
