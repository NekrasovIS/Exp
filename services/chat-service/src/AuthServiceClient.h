#pragma once

#include <optional>
#include <string>

namespace chat_service {

/**
 * @brief Вызывает POST /auth/verify auth-service для разрешения токена.
 *
 * Отказывает закрыто (fail closed): любая сетевая/протокольная ошибка
 * или недействительный/просроченный токен трактуются как "не
 * аутентифицирован", а не приводят к исключению.
 */
class AuthServiceClient {
public:
    AuthServiceClient(std::string host, int port);

    /// @return Субъект (логин) токена, если он действителен, иначе std::nullopt.
    [[nodiscard]] std::optional<std::string> verifyToken(const std::string& token) const;

private:
    std::string host_;
    int port_;
};

}  // namespace chat_service
