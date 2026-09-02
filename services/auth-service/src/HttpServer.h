#pragma once

#include <httplib.h>

#include <chrono>
#include <string>

#include "RateLimiter.h"
#include "TokenService.h"
#include "UserServiceClient.h"

namespace auth_service {

/**
 * @brief REST-обёртка над TokenService: POST /auth/token,
 *        POST /auth/verify, POST /auth/register, POST /auth/refresh.
 *
 * POST /auth/token требует валидные {"login", "password"} — они
 * проверяются через user-service с помощью UserServiceClient — прежде
 * чем выдать токен. POST /auth/register перенаправляет запрос в
 * собственную регистрацию user-service и, в случае успеха, сразу же
 * выдаёт токен (авто-логин), чтобы клиенту не требовался второй запрос.
 * Оба метода также возвращают долгоживущий refresh_token (issue #105)
 * вместе с access-токеном; POST /auth/refresh обменивает ещё
 * действительный refresh-токен на свежий access-токен через
 * {"refresh_token"} — без повторного ввода учётных данных пользователем
 * — с тем же форматом ответа, что и /auth/token. В остальном — тонкая
 * обёртка над httplib::Server: вся логика токенов находится в
 * TokenService, этот класс лишь транслирует HTTP-запросы/ответы.
 *
 * /auth/token и /auth/register (оба проверяют учётные данные)
 * ограничены по частоте запросов на клиентский адрес (issue #102) —
 * /auth/verify и /auth/refresh не ограничены: /auth/verify легитимно
 * вызывается chat-service на каждый обрабатываемый им запрос, а
 * /auth/refresh требует уже валидного подписанного refresh-токена, а не
 * угадываемых учётных данных, поэтому ни один из них не должен
 * попадать под лимитер, предназначенный для защиты от brute-force.
 */
class HttpServer {
public:
    /// @p rateLimitMaxRequests/@p rateLimitWindow настраивают лимитер
    /// для /auth/token + /auth/register — значения по умолчанию — это
    /// реальный продакшн-лимит; тесты передают крошечное окно, чтобы
    /// сработать быстро и детерминированно, не дожидаясь реальных часов.
    HttpServer(const TokenService& tokenService, const UserServiceClient& userServiceClient,
               int rateLimitMaxRequests = 10, std::chrono::milliseconds rateLimitWindow = std::chrono::seconds{60});

    /// Блокирует поток, обслуживая запросы, пока stop() не будет вызван из другого потока.
    void listen(const std::string& host, int port);

    /// Останавливает выполняющийся вызов listen().
    void stop();

private:
    void registerRoutes();
    void handleIssueToken(const httplib::Request& request, httplib::Response& response);
    void handleVerifyToken(const httplib::Request& request, httplib::Response& response);
    void handleRegister(const httplib::Request& request, httplib::Response& response);
    void handleRefresh(const httplib::Request& request, httplib::Response& response);

    const TokenService& tokenService_;
    const UserServiceClient& userServiceClient_;
    RateLimiter rateLimiter_;
    httplib::Server server_;
};

}  // namespace auth_service
