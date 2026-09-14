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
 *        AuthServiceClient у auth-service. Плюс
 *        GET /internal/friendship?user_a=&user_b= (issue #187, Фаза 2) —
 *        без аутентификации, как и /users/resolve-otp-identifier: не
 *        вызывается напрямую клиентами, только chat-service, чтобы
 *        решить, можно ли открыть новый диалог личных сообщений. Плюс
 *        POST /profile/avatar (аутентифицированный) и
 *        GET /users/{login}/avatar (issue #384) — последний нарочно БЕЗ
 *        аутентификации, единственное исключение в этом списке: это
 *        конечная точка `<img src="...">`, а браузер не может приложить
 *        заголовок Authorization к запросу картинки, инициированному
 *        атрибутом src напрямую. Тот же уровень доверия, что и у
 *        public_key в toPublicJson() — публично для любого, кто знает
 *        логин.
 *
 * PATCH /users/me всегда пишет в аккаунт, чей login зашит в токене —
 * login в URL/теле запроса, если есть, игнорируется, поэтому вызывающая
 * сторона никогда не может отредактировать чужой профиль.
 *
 * GET /users/{login}/profile (issue #225, pentest): отдаёт email/
 * telegram_chat_id, только когда {login} совпадает с логином из
 * токена вызывающего — просмотр чужого профиля получает урезанную
 * версию без этих полей (см. toPublicJson()/toJson() в HttpServer.cpp).
 *
 * Тонкая обёртка над httplib::Server — вся бизнес-логика аккаунтов
 * живёт в UserService, этот класс только переводит HTTP-запросы/ответы.
 */
class HttpServer {
public:
    /// @p corsAllowedOrigin — issue #354: см. doc-комментарий на
    /// одноимённом параметре chat-service's HttpServer::HttpServer()
    /// (тот же паттерн CORS для веб-клиента). По умолчанию — адрес Vite
    /// dev server для локальной разработки.
    HttpServer(UserService& userService, const AuthServiceClient& authServiceClient,
               const std::string& corsAllowedOrigin = "http://localhost:5173");

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
    void handleCheckFriendship(const httplib::Request& request, httplib::Response& response);
    void handleUploadAvatar(const httplib::Request& request, httplib::Response& response);
    void handleGetAvatar(const httplib::Request& request, httplib::Response& response);

    UserService& userService_;
    const AuthServiceClient& authServiceClient_;
    httplib::Server server_;
};

}  // namespace user_service
