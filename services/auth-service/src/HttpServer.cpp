#include "HttpServer.h"

#include "JsonGuard.h"

#include <nlohmann/json.hpp>

namespace auth_service {

namespace {
constexpr const char* kJsonContentType = "application/json";
// Тот же выбор компромисса, что и у RateLimiter/TokenService (issue
// #102/#105) — 5 минут достаточно, чтобы код дошёл и был введён, но не
// настолько долго, чтобы расширять окно перебора.
constexpr std::chrono::seconds kOtpTtl{300};
constexpr int kOtpMaxAttempts = 5;
}  // namespace

HttpServer::HttpServer(const TokenService& tokenService, const UserServiceClient& userServiceClient,
                        const ICodeDeliveryChannel& codeDeliveryChannel, const ICodeDeliveryChannel* telegramChannel,
                        int rateLimitMaxRequests, std::chrono::milliseconds rateLimitWindow)
    : tokenService_(tokenService),
      userServiceClient_(userServiceClient),
      codeDeliveryChannel_(codeDeliveryChannel),
      telegramChannel_(telegramChannel),
      rateLimiter_(rateLimitMaxRequests, rateLimitWindow),
      otpStore_(kOtpTtl, kOtpMaxAttempts) {
    registerRoutes();
}

void HttpServer::registerRoutes() {
    server_.Post("/auth/token", [this](const httplib::Request& request, httplib::Response& response) {
        handleIssueToken(request, response);
    });
    server_.Post("/auth/verify", [this](const httplib::Request& request, httplib::Response& response) {
        handleVerifyToken(request, response);
    });
    server_.Post("/auth/register", [this](const httplib::Request& request, httplib::Response& response) {
        handleRegister(request, response);
    });
    server_.Post("/auth/refresh", [this](const httplib::Request& request, httplib::Response& response) {
        handleRefresh(request, response);
    });
    server_.Post("/auth/otp/request", [this](const httplib::Request& request, httplib::Response& response) {
        handleOtpRequest(request, response);
    });
    server_.Post("/auth/otp/verify", [this](const httplib::Request& request, httplib::Response& response) {
        handleOtpVerify(request, response);
    });
}

void HttpServer::handleIssueToken(const httplib::Request& request, httplib::Response& response) {
    if (!rateLimiter_.allow(request.remote_addr)) {
        response.status = 429;
        response.set_content(nlohmann::json{{"error", "too many requests, try again later"}}.dump(),
                              kJsonContentType);
        return;
    }

    if (json_guard::exceedsMaxNestingDepth(request.body, json_guard::kMaxNestingDepth)) {
        response.status = 400;
        response.set_content(nlohmann::json{{"error", "payload too deeply nested"}}.dump(), kJsonContentType);
        return;
    }
    const nlohmann::json body = nlohmann::json::parse(request.body, nullptr, /*allow_exceptions=*/false);
    if (body.is_discarded() || !body.contains("login") || !body.contains("password") ||
        !body["login"].is_string() || !body["password"].is_string()) {
        response.status = 400;
        response.set_content(nlohmann::json{{"error", "expected 'login' and 'password' strings"}}.dump(),
                              kJsonContentType);
        return;
    }

    const std::string login = body["login"].get<std::string>();
    if (!userServiceClient_.verifyCredentials(login, body["password"].get<std::string>())) {
        response.status = 401;
        response.set_content(nlohmann::json{{"error", "invalid credentials"}}.dump(), kJsonContentType);
        return;
    }

    const Token token = tokenService_.issueToken(login);
    const Token refreshToken = tokenService_.issueRefreshToken(login);
    const nlohmann::json responseBody{
        {"token", token.value}, {"expires_at", token.expiresAt}, {"refresh_token", refreshToken.value}};
    response.set_content(responseBody.dump(), kJsonContentType);
}

void HttpServer::handleVerifyToken(const httplib::Request& request, httplib::Response& response) {
    if (json_guard::exceedsMaxNestingDepth(request.body, json_guard::kMaxNestingDepth)) {
        response.status = 400;
        response.set_content(nlohmann::json{{"error", "payload too deeply nested"}}.dump(), kJsonContentType);
        return;
    }
    const nlohmann::json body = nlohmann::json::parse(request.body, nullptr, /*allow_exceptions=*/false);
    if (body.is_discarded() || !body.contains("token") || !body["token"].is_string()) {
        response.status = 400;
        response.set_content(nlohmann::json{{"error", "missing 'token' field"}}.dump(), kJsonContentType);
        return;
    }

    if (const std::optional<std::string> subject = tokenService_.verifyToken(body["token"].get<std::string>());
        subject.has_value()) {
        response.set_content(nlohmann::json{{"valid", true}, {"subject", *subject}}.dump(), kJsonContentType);
    } else {
        response.set_content(nlohmann::json{{"valid", false}}.dump(), kJsonContentType);
    }
}

void HttpServer::handleRegister(const httplib::Request& request, httplib::Response& response) {
    if (!rateLimiter_.allow(request.remote_addr)) {
        response.status = 429;
        response.set_content(nlohmann::json{{"error", "too many requests, try again later"}}.dump(),
                              kJsonContentType);
        return;
    }

    if (json_guard::exceedsMaxNestingDepth(request.body, json_guard::kMaxNestingDepth)) {
        response.status = 400;
        response.set_content(nlohmann::json{{"error", "payload too deeply nested"}}.dump(), kJsonContentType);
        return;
    }
    const nlohmann::json body = nlohmann::json::parse(request.body, nullptr, /*allow_exceptions=*/false);
    if (body.is_discarded() || !body.contains("login") || !body.contains("password") ||
        !body["login"].is_string() || !body["password"].is_string()) {
        response.status = 400;
        response.set_content(nlohmann::json{{"error", "expected 'login' and 'password' strings"}}.dump(),
                              kJsonContentType);
        return;
    }

    const std::string login = body["login"].get<std::string>();
    if (!userServiceClient_.registerUser(login, body["password"].get<std::string>())) {
        response.status = 409;
        response.set_content(nlohmann::json{{"registered", false}}.dump(), kJsonContentType);
        return;
    }

    const Token token = tokenService_.issueToken(login);
    const Token refreshToken = tokenService_.issueRefreshToken(login);
    response.status = 201;
    response.set_content(nlohmann::json{{"registered", true},
                                         {"token", token.value},
                                         {"expires_at", token.expiresAt},
                                         {"refresh_token", refreshToken.value}}
                              .dump(),
                          kJsonContentType);
}

void HttpServer::handleRefresh(const httplib::Request& request, httplib::Response& response) {
    if (json_guard::exceedsMaxNestingDepth(request.body, json_guard::kMaxNestingDepth)) {
        response.status = 400;
        response.set_content(nlohmann::json{{"error", "payload too deeply nested"}}.dump(), kJsonContentType);
        return;
    }
    const nlohmann::json body = nlohmann::json::parse(request.body, nullptr, /*allow_exceptions=*/false);
    if (body.is_discarded() || !body.contains("refresh_token") || !body["refresh_token"].is_string()) {
        response.status = 400;
        response.set_content(nlohmann::json{{"error", "expected 'refresh_token' string"}}.dump(), kJsonContentType);
        return;
    }

    const std::optional<std::string> subject =
        tokenService_.verifyRefreshToken(body["refresh_token"].get<std::string>());
    if (!subject.has_value()) {
        response.status = 401;
        response.set_content(nlohmann::json{{"error", "invalid or expired refresh token"}}.dump(), kJsonContentType);
        return;
    }

    const Token token = tokenService_.issueToken(*subject);
    response.set_content(nlohmann::json{{"token", token.value}, {"expires_at", token.expiresAt}}.dump(),
                          kJsonContentType);
}

void HttpServer::handleOtpRequest(const httplib::Request& request, httplib::Response& response) {
    if (!rateLimiter_.allow(request.remote_addr)) {
        response.status = 429;
        response.set_content(nlohmann::json{{"error", "too many requests, try again later"}}.dump(),
                              kJsonContentType);
        return;
    }

    if (json_guard::exceedsMaxNestingDepth(request.body, json_guard::kMaxNestingDepth)) {
        response.status = 400;
        response.set_content(nlohmann::json{{"error", "payload too deeply nested"}}.dump(), kJsonContentType);
        return;
    }
    const nlohmann::json body = nlohmann::json::parse(request.body, nullptr, /*allow_exceptions=*/false);
    if (body.is_discarded() || !body.contains("identifier") || !body["identifier"].is_string()) {
        response.status = 400;
        response.set_content(nlohmann::json{{"error", "expected an 'identifier' string"}}.dump(), kJsonContentType);
        return;
    }

    // Всегда отвечаем 200 независимо от того, нашёлся ли реальный
    // аккаунт — иначе ответ сам по себе становится способом проверить,
    // какие email/login/telegram_chat_id существуют в системе.
    if (const auto resolved = userServiceClient_.resolveOtpIdentifier(body["identifier"].get<std::string>());
        resolved.has_value()) {
        const std::string code = otpStore_.issue(resolved->login);
        // Telegram предпочтительнее email, когда у аккаунта задано и
        // то и другое, а на сервере настроен TELEGRAM_BOT_TOKEN —
        // мгновенная доставка в чат, а не письмо, которое может уйти в
        // спам/прийти с задержкой.
        if (telegramChannel_ != nullptr && resolved->telegramChatId.has_value()) {
            telegramChannel_->send(*resolved->telegramChatId, code);
        } else if (resolved->email.has_value()) {
            codeDeliveryChannel_.send(*resolved->email, code);
        }
    }
    response.set_content(nlohmann::json{{"sent", true}}.dump(), kJsonContentType);
}

void HttpServer::handleOtpVerify(const httplib::Request& request, httplib::Response& response) {
    if (!rateLimiter_.allow(request.remote_addr)) {
        response.status = 429;
        response.set_content(nlohmann::json{{"error", "too many requests, try again later"}}.dump(),
                              kJsonContentType);
        return;
    }

    if (json_guard::exceedsMaxNestingDepth(request.body, json_guard::kMaxNestingDepth)) {
        response.status = 400;
        response.set_content(nlohmann::json{{"error", "payload too deeply nested"}}.dump(), kJsonContentType);
        return;
    }
    const nlohmann::json body = nlohmann::json::parse(request.body, nullptr, /*allow_exceptions=*/false);
    if (body.is_discarded() || !body.contains("identifier") || !body.contains("code") ||
        !body["identifier"].is_string() || !body["code"].is_string()) {
        response.status = 400;
        response.set_content(nlohmann::json{{"error", "expected 'identifier' and 'code' strings"}}.dump(),
                              kJsonContentType);
        return;
    }

    const auto resolved = userServiceClient_.resolveOtpIdentifier(body["identifier"].get<std::string>());
    if (!resolved.has_value() || !otpStore_.verify(resolved->login, body["code"].get<std::string>())) {
        response.status = 401;
        response.set_content(nlohmann::json{{"error", "invalid or expired code"}}.dump(), kJsonContentType);
        return;
    }

    const std::string& login = resolved->login;
    const Token token = tokenService_.issueToken(login);
    const Token refreshToken = tokenService_.issueRefreshToken(login);
    response.set_content(
        nlohmann::json{{"token", token.value}, {"expires_at", token.expiresAt}, {"refresh_token", refreshToken.value}}
            .dump(),
        kJsonContentType);
}

void HttpServer::listen(const std::string& host, int port) {
    server_.listen(host, port);
}

void HttpServer::stop() {
    server_.stop();
}

}  // namespace auth_service
