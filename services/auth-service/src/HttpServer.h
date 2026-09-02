#pragma once

#include <httplib.h>

#include <chrono>
#include <string>

#include "CodeDeliveryChannel.h"
#include "OtpStore.h"
#include "RateLimiter.h"
#include "TokenService.h"
#include "UserServiceClient.h"

namespace auth_service {

/**
 * @brief REST-фасад над TokenService: POST /auth/token,
 *        POST /auth/verify, POST /auth/register, POST /auth/refresh,
 *        POST /auth/otp/request, POST /auth/otp/verify (issue #156).
 *
 * POST /auth/token требует валидные {"login", "password"} — проверяются
 * через user-service (UserServiceClient) — прежде чем выдать токен.
 * POST /auth/register перенаправляет в собственную регистрацию
 * user-service и, при успехе, сразу же выдаёт токен (авто-логин), чтобы
 * клиенту не требовался второй раунд запросов. Оба также возвращают
 * долгоживущий refresh_token (issue #105) вместе с access-токеном;
 * POST /auth/refresh обменивает ещё действующий refresh-токен на
 * свежий access-токен через {"refresh_token"} — без повторного ввода
 * учётных данных пользователем — та же форма ответа, что у /auth/token.
 *
 * POST /auth/otp/request принимает {"identifier"} (login, email или
 * Telegram chat_id, issue #156/#174) — всегда отвечает 200 независимо
 * от того, находится ли реальный аккаунт по этому идентификатору,
 * чтобы ответ нельзя было использовать для проверки существования
 * аккаунта; если идентификатор находится, код доставляется через
 * telegramChannel_ (если у аккаунта задан telegram_chat_id и канал
 * настроен на сервере), иначе через codeDeliveryChannel_ (email), если
 * задан email — предпочтение Telegram над email, когда оба доступны.
 * POST /auth/otp/verify принимает {"identifier", "code"} и при
 * совпадении ещё действующего кода выдаёт пару токен/refresh-токен —
 * та же форма ответа, что у /auth/token.
 *
 * В остальном — тонкая обёртка над httplib::Server: вся логика токенов
 * живёт в TokenService, этот класс только переводит HTTP-запросы/ответы.
 *
 * /auth/token, /auth/register, /auth/otp/request и /auth/otp/verify (все
 * проверяющие учётные данные/код) ограничены по частоте на клиентский
 * адрес (issue #102) — /auth/verify и /auth/refresh нет: /auth/verify
 * легитимно вызывается chat-service на каждый обрабатываемый им запрос,
 * а /auth/refresh требует уже валидный подписанный refresh-токен, а не
 * угадываемые учётные данные, так что ни тот ни другой не должны
 * попадать под лимитер, предназначенный против перебора.
 */
class HttpServer {
public:
    /// @p telegramChannel — nullptr, если TELEGRAM_BOT_TOKEN не
    /// настроен на сервере (issue #174); тогда доставка всегда идёт
    /// через @p codeDeliveryChannel (email), как и без Telegram-
    /// поддержки. @p rateLimitMaxRequests/@p rateLimitWindow настраивают
    /// лимитер для /auth/token + /auth/register + /auth/otp/* — значения
    /// по умолчанию рассчитаны на прод; тесты передают крошечное окно,
    /// чтобы сработать быстро и детерминированно, а не ждать реальные
    /// часы.
    HttpServer(const TokenService& tokenService, const UserServiceClient& userServiceClient,
               const ICodeDeliveryChannel& codeDeliveryChannel, const ICodeDeliveryChannel* telegramChannel = nullptr,
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
    void handleOtpRequest(const httplib::Request& request, httplib::Response& response);
    void handleOtpVerify(const httplib::Request& request, httplib::Response& response);

    const TokenService& tokenService_;
    const UserServiceClient& userServiceClient_;
    const ICodeDeliveryChannel& codeDeliveryChannel_;
    const ICodeDeliveryChannel* telegramChannel_;
    RateLimiter rateLimiter_;
    OtpStore otpStore_;
    httplib::Server server_;
};

}  // namespace auth_service
