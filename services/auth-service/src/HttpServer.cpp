#include "HttpServer.h"

#include <nlohmann/json.hpp>

namespace auth_service {

namespace {
constexpr const char* kJsonContentType = "application/json";
}  // namespace

HttpServer::HttpServer(const TokenService& tokenService, const UserServiceClient& userServiceClient)
    : tokenService_(tokenService), userServiceClient_(userServiceClient) {
    registerRoutes();
}

void HttpServer::registerRoutes() {
    server_.Post("/auth/token", [this](const httplib::Request& request, httplib::Response& response) {
        handleIssueToken(request, response);
    });
    server_.Post("/auth/verify", [this](const httplib::Request& request, httplib::Response& response) {
        handleVerifyToken(request, response);
    });
}

void HttpServer::handleIssueToken(const httplib::Request& request, httplib::Response& response) {
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
    const nlohmann::json responseBody{{"token", token.value}, {"expires_at", token.expiresAt}};
    response.set_content(responseBody.dump(), kJsonContentType);
}

void HttpServer::handleVerifyToken(const httplib::Request& request, httplib::Response& response) {
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

void HttpServer::listen(const std::string& host, int port) {
    server_.listen(host, port);
}

void HttpServer::stop() {
    server_.stop();
}

}  // namespace auth_service
