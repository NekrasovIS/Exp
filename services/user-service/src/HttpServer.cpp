#include "HttpServer.h"

#include <nlohmann/json.hpp>

#include <optional>

namespace user_service {

namespace {
constexpr const char* kJsonContentType = "application/json";

// Both endpoints take the same {"login", "password"} shape.
struct Credentials {
    std::string login;
    std::string password;
};

std::optional<Credentials> parseCredentials(const std::string& body) {
    const nlohmann::json json = nlohmann::json::parse(body, nullptr, /*allow_exceptions=*/false);
    if (json.is_discarded() || !json.contains("login") || !json.contains("password") ||
        !json["login"].is_string() || !json["password"].is_string()) {
        return std::nullopt;
    }
    return Credentials{.login = json["login"].get<std::string>(), .password = json["password"].get<std::string>()};
}
}  // namespace

HttpServer::HttpServer(UserService& userService) : userService_(userService) {
    registerRoutes();
}

void HttpServer::registerRoutes() {
    server_.Post("/users/register", [this](const httplib::Request& request, httplib::Response& response) {
        handleRegister(request, response);
    });
    server_.Post("/users/verify-credentials", [this](const httplib::Request& request, httplib::Response& response) {
        handleVerifyCredentials(request, response);
    });
}

void HttpServer::handleRegister(const httplib::Request& request, httplib::Response& response) {
    const std::optional<Credentials> credentials = parseCredentials(request.body);
    if (!credentials.has_value()) {
        response.status = 400;
        response.set_content(nlohmann::json{{"error", "expected 'login' and 'password' strings"}}.dump(),
                              kJsonContentType);
        return;
    }

    const bool registered = userService_.registerUser(credentials->login, credentials->password);
    response.status = registered ? 201 : 409;
    response.set_content(nlohmann::json{{"registered", registered}}.dump(), kJsonContentType);
}

void HttpServer::handleVerifyCredentials(const httplib::Request& request, httplib::Response& response) {
    const std::optional<Credentials> credentials = parseCredentials(request.body);
    if (!credentials.has_value()) {
        response.status = 400;
        response.set_content(nlohmann::json{{"error", "expected 'login' and 'password' strings"}}.dump(),
                              kJsonContentType);
        return;
    }

    const bool valid = userService_.verifyCredentials(credentials->login, credentials->password);
    response.set_content(nlohmann::json{{"valid", valid}}.dump(), kJsonContentType);
}

void HttpServer::listen(const std::string& host, int port) {
    server_.listen(host, port);
}

void HttpServer::stop() {
    server_.stop();
}

}  // namespace user_service
