#pragma once

#include <httplib.h>

#include <optional>
#include <string>

#include "AuthServiceClient.h"
#include "UserService.h"

namespace user_service {

/**
 * @brief REST-фасад над UserService: POST /users/register,
 *        POST /users/verify-credentials, POST /users/resolve-otp-identifier
 *        (issue #156, все три без аутентификации — вызываются самим
 *        auth-service, а не напрямую клиентами), плюс
 *        GET /users/{login}/profile и PATCH /users/me (issue #110),
 *        POST /friends/requests, GET /friends/requests,
 *        POST /friends/requests/{id}/accept,
 *        POST /friends/requests/{id}/decline, GET /friends,
 *        DELETE /friends/{login} (issue #187, заявки в друзья) —
 *        которым нужен валидный заголовок
 *        `Authorization: Bearer <token>`, проверяемый через
 *        AuthServiceClient у auth-service.
 *
 * PATCH /users/me всегда пишет в аккаунт, чей login зашит в токене —
 * login в URL/теле запроса, если есть, игнорируется, поэтому вызывающая
 * сторона никогда не может отредактировать чужой профиль.
 *
 * Тонкая обёртка над httplib::Server — вся бизнес-логика аккаунтов
 * живёт в UserService, этот класс только переводит HTTP-запросы/ответы.
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
    void handleResolveOtpIdentifier(const httplib::Request& request, httplib::Response& response);
    void handleSendFriendRequest(const httplib::Request& request, httplib::Response& response);
    void handleListIncomingFriendRequests(const httplib::Request& request, httplib::Response& response);
    void handleAcceptFriendRequest(const httplib::Request& request, httplib::Response& response);
    void handleDeclineFriendRequest(const httplib::Request& request, httplib::Response& response);
    void handleListFriends(const httplib::Request& request, httplib::Response& response);
    void handleRemoveFriend(const httplib::Request& request, httplib::Response& response);

    UserService& userService_;
    const AuthServiceClient& authServiceClient_;
    httplib::Server server_;
};

}  // namespace user_service
