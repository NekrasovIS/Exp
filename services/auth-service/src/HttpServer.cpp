#include "HttpServer.h"

#include <nlohmann/json.hpp>

namespace auth_service {

namespace {
constexpr const char* kJsonContentType = "application/json";
constexpr const char* kDefaultSubject = "devicehub-client";
}  // namespace

HttpServer::HttpServer(const TokenService& tokenService) : tokenService_(tokenService) {
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
    std::string subject = kDefaultSubject;

    if (const nlohmann::json body = nlohmann::json::parse(request.body, nullptr, /*allow_exceptions=*/false);
        !body.is_discarded() && body.contains("subject") && body["subject"].is_string()) {
        subject = body["subject"].get<std::string>();
    }

    const Token token = tokenService_.issueToken(subject);
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
