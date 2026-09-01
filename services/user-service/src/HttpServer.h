#pragma once

#include <httplib.h>

#include <optional>
#include <string>

#include "AuthServiceClient.h"
#include "UserService.h"

namespace user_service {

/**
 * @brief REST-фасад над UserService: POST /users/register,
 *        POST /users/verify-credentials (оба без аутентификации — их вызывает
 *        сам auth-service, а не напрямую клиенты конечных пользователей), а также
 *        GET /users/{login}/profile и PATCH /users/me (issue #110),
 *        которые требуют действительный заголовок `Authorization: Bearer <token>`,
 *        проверяемый через AuthServiceClient у auth-service.
 *
 * PATCH /users/me всегда пишет в собственный субъект токена — логин
 * в URL/теле запроса, если он указан, игнорируется, поэтому вызывающий
 * не может отредактировать чужой профиль.
 *
 * Тонкая обёртка над httplib::Server — вся логика работы с аккаунтами
 * находится в UserService, этот класс лишь транслирует HTTP-запросы/ответы.
 */
class HttpServer {
public:
    HttpServer(UserService& userService, const AuthServiceClient& authServiceClient);

    /// Блокирует выполнение, обслуживая запросы, пока stop() не будет вызван из другого потока.
    void listen(const std::string& host, int port);

    /// Останавливает выполняющийся вызов listen().
    void stop();

private:
    void registerRoutes();
    [[nodiscard]] std::optional<std::string> authenticate(const httplib::Request& request) const;

    void handleRegister(const httplib::Request& request, httplib::Response& response);
    void handleVerifyCredentials(const httplib::Request& request, httplib::Response& response);
    void handleGetProfile(const httplib::Request& request, httplib::Response& response);
    void handleUpdateOwnProfile(const httplib::Request& request, httplib::Response& response);

    UserService& userService_;
    const AuthServiceClient& authServiceClient_;
    httplib::Server server_;
};

}  // namespace user_service
