#include "HttpServer.h"

#include "JsonGuard.h"

#include <nlohmann/json.hpp>

#include <optional>
#include <string_view>
#include <utility>

namespace user_service {

namespace {
constexpr const char* kJsonContentType = "application/json";
constexpr std::string_view kBearerPrefix = "Bearer ";

// Оба эндпоинта принимают одинаковую форму {"login", "password"}.
struct Credentials {
    std::string login;
    std::string password;
};

std::optional<Credentials> parseCredentials(const std::string& body) {
    if (json_guard::exceedsMaxNestingDepth(body, json_guard::kMaxNestingDepth)) {
        return std::nullopt;
    }
    const nlohmann::json json = nlohmann::json::parse(body, nullptr, /*allow_exceptions=*/false);
    if (json.is_discarded() || !json.contains("login") || !json.contains("password") ||
        !json["login"].is_string() || !json["password"].is_string()) {
        return std::nullopt;
    }
    return Credentials{.login = json["login"].get<std::string>(), .password = json["password"].get<std::string>()};
}

nlohmann::json toJson(const Profile& profile) {
    return nlohmann::json{{"login", profile.login},
                           {"display_name", profile.displayName.has_value() ? nlohmann::json(*profile.displayName)
                                                                             : nlohmann::json(nullptr)},
                           {"avatar_url", profile.avatarUrl.has_value() ? nlohmann::json(*profile.avatarUrl)
                                                                         : nlohmann::json(nullptr)},
                           {"public_key", profile.publicKey.has_value() ? nlohmann::json(*profile.publicKey)
                                                                         : nlohmann::json(nullptr)},
                           {"email",
                            profile.email.has_value() ? nlohmann::json(*profile.email) : nlohmann::json(nullptr)}};
}

std::optional<std::string> parseIdentifier(const std::string& body) {
    if (json_guard::exceedsMaxNestingDepth(body, json_guard::kMaxNestingDepth)) {
        return std::nullopt;
    }
    const nlohmann::json json = nlohmann::json::parse(body, nullptr, /*allow_exceptions=*/false);
    if (json.is_discarded() || !json.contains("identifier") || !json["identifier"].is_string()) {
        return std::nullopt;
    }
    return json["identifier"].get<std::string>();
}
}  // namespace

HttpServer::HttpServer(UserService& userService, const AuthServiceClient& authServiceClient)
    : userService_(userService), authServiceClient_(authServiceClient) {
    registerRoutes();
}

std::optional<std::string> HttpServer::authenticate(const httplib::Request& request) const {
    const std::string header = request.get_header_value("Authorization");
    if (header.size() <= kBearerPrefix.size() || header.compare(0, kBearerPrefix.size(), kBearerPrefix) != 0) {
        return std::nullopt;
    }
    return authServiceClient_.verifyToken(header.substr(kBearerPrefix.size()));
}

void HttpServer::registerRoutes() {
    server_.Post("/users/register", [this](const httplib::Request& request, httplib::Response& response) {
        handleRegister(request, response);
    });
    server_.Post("/users/verify-credentials", [this](const httplib::Request& request, httplib::Response& response) {
        handleVerifyCredentials(request, response);
    });
    server_.Get(R"(/users/([^/]+)/profile)", [this](const httplib::Request& request, httplib::Response& response) {
        handleGetProfile(request, response);
    });
    server_.Patch("/users/me", [this](const httplib::Request& request, httplib::Response& response) {
        handleUpdateOwnProfile(request, response);
    });
    server_.Post("/users/resolve-otp-identifier", [this](const httplib::Request& request, httplib::Response& response) {
        handleResolveOtpIdentifier(request, response);
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

void HttpServer::handleGetProfile(const httplib::Request& request, httplib::Response& response) {
    if (!authenticate(request).has_value()) {
        response.status = 401;
        return;
    }

    const std::optional<Profile> profile = userService_.getProfile(request.matches[1].str());
    if (!profile.has_value()) {
        response.status = 404;
        response.set_content(nlohmann::json{{"error", "no such user"}}.dump(), kJsonContentType);
        return;
    }
    response.set_content(toJson(*profile).dump(), kJsonContentType);
}

void HttpServer::handleUpdateOwnProfile(const httplib::Request& request, httplib::Response& response) {
    const std::optional<std::string> login = authenticate(request);
    if (!login.has_value()) {
        response.status = 401;
        return;
    }

    if (json_guard::exceedsMaxNestingDepth(request.body, json_guard::kMaxNestingDepth)) {
        response.status = 400;
        response.set_content(nlohmann::json{{"error", "payload too deeply nested"}}.dump(), kJsonContentType);
        return;
    }
    const nlohmann::json body = nlohmann::json::parse(request.body, nullptr, /*allow_exceptions=*/false);
    if (body.is_discarded()) {
        response.status = 400;
        response.set_content(nlohmann::json{{"error", "malformed JSON"}}.dump(), kJsonContentType);
        return;
    }
    // Частичное обновление: поле, отсутствующее в теле запроса, сохраняет своё
    // текущее значение, а не очищается — сначала читаем, затем сливаем, поскольку
    // UserRepository::updateProfile() всегда записывает все три столбца.
    const std::optional<Profile> current = userService_.getProfile(*login);
    ProfileUpdate update{.displayName = current.has_value() ? current->displayName : std::nullopt,
                          .avatarUrl = current.has_value() ? current->avatarUrl : std::nullopt,
                          .publicKey = current.has_value() ? current->publicKey : std::nullopt,
                          .email = current.has_value() ? current->email : std::nullopt};
    if (body.contains("display_name") && body["display_name"].is_string()) {
        update.displayName = body["display_name"].get<std::string>();
    }
    if (body.contains("avatar_url") && body["avatar_url"].is_string()) {
        update.avatarUrl = body["avatar_url"].get<std::string>();
    }
    if (body.contains("public_key") && body["public_key"].is_string()) {
        update.publicKey = body["public_key"].get<std::string>();
    }
    if (body.contains("email") && body["email"].is_string()) {
        update.email = body["email"].get<std::string>();
    }

    switch (userService_.updateProfile(*login, update)) {
        using enum UpdateProfileResult;
        case kNoSuchUser:
            response.status = 404;
            response.set_content(nlohmann::json{{"error", "no such user"}}.dump(), kJsonContentType);
            return;
        case kEmailTaken:
            response.status = 409;
            response.set_content(nlohmann::json{{"error", "email already in use"}}.dump(), kJsonContentType);
            return;
        case kUpdated:
            break;
    }
    response.set_content(
        toJson(Profile{.login = *login,
                        .displayName = update.displayName,
                        .avatarUrl = update.avatarUrl,
                        .publicKey = update.publicKey,
                        .email = update.email})
            .dump(),
        kJsonContentType);
}

void HttpServer::handleResolveOtpIdentifier(const httplib::Request& request, httplib::Response& response) {
    const std::optional<std::string> identifier = parseIdentifier(request.body);
    if (!identifier.has_value()) {
        response.status = 400;
        response.set_content(nlohmann::json{{"error", "expected an 'identifier' string"}}.dump(), kJsonContentType);
        return;
    }

    const std::optional<std::pair<std::string, std::string>> resolved =
        userService_.resolveOtpIdentifier(*identifier);
    if (!resolved.has_value()) {
        response.set_content(nlohmann::json{{"found", false}}.dump(), kJsonContentType);
        return;
    }
    response.set_content(
        nlohmann::json{{"found", true}, {"login", resolved->first}, {"email", resolved->second}}.dump(),
        kJsonContentType);
}

void HttpServer::listen(const std::string& host, int port) {
    server_.listen(host, port);
}

void HttpServer::stop() {
    server_.stop();
}

}  // namespace user_service
