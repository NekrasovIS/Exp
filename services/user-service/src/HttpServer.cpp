#include "HttpServer.h"

#include "JsonGuard.h"

#include <nlohmann/json.hpp>

#include <optional>
#include <string_view>

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
    return nlohmann::json{
        {"login", profile.login},
        {"display_name",
         profile.displayName.has_value() ? nlohmann::json(*profile.displayName) : nlohmann::json(nullptr)},
        {"avatar_url",
         profile.avatarUrl.has_value() ? nlohmann::json(*profile.avatarUrl) : nlohmann::json(nullptr)},
        {"public_key",
         profile.publicKey.has_value() ? nlohmann::json(*profile.publicKey) : nlohmann::json(nullptr)},
        {"email", profile.email.has_value() ? nlohmann::json(*profile.email) : nlohmann::json(nullptr)},
        {"telegram_chat_id", profile.telegramChatId.has_value() ? nlohmann::json(*profile.telegramChatId)
                                                                  : nlohmann::json(nullptr)}};
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

std::optional<std::string> parseRecipientLogin(const std::string& body) {
    if (json_guard::exceedsMaxNestingDepth(body, json_guard::kMaxNestingDepth)) {
        return std::nullopt;
    }
    const nlohmann::json json = nlohmann::json::parse(body, nullptr, /*allow_exceptions=*/false);
    if (json.is_discarded() || !json.contains("recipient_login") || !json["recipient_login"].is_string()) {
        return std::nullopt;
    }
    return json["recipient_login"].get<std::string>();
}

nlohmann::json toJson(const FriendRequestInfo& request) {
    return nlohmann::json{
        {"id", request.id}, {"requester_login", request.requesterLogin}, {"created_at", request.createdAt}};
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
    server_.Post("/friends/requests", [this](const httplib::Request& request, httplib::Response& response) {
        handleSendFriendRequest(request, response);
    });
    server_.Get("/friends/requests", [this](const httplib::Request& request, httplib::Response& response) {
        handleListIncomingFriendRequests(request, response);
    });
    server_.Post(R"(/friends/requests/(\d+)/accept)",
                  [this](const httplib::Request& request, httplib::Response& response) {
                      handleAcceptFriendRequest(request, response);
                  });
    server_.Post(R"(/friends/requests/(\d+)/decline)",
                  [this](const httplib::Request& request, httplib::Response& response) {
                      handleDeclineFriendRequest(request, response);
                  });
    server_.Get("/friends", [this](const httplib::Request& request, httplib::Response& response) {
        handleListFriends(request, response);
    });
    server_.Delete(R"(/friends/([^/]+))", [this](const httplib::Request& request, httplib::Response& response) {
        handleRemoveFriend(request, response);
    });
    server_.Get("/internal/friendship", [this](const httplib::Request& request, httplib::Response& response) {
        handleCheckFriendship(request, response);
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
                          .email = current.has_value() ? current->email : std::nullopt,
                          .telegramChatId = current.has_value() ? current->telegramChatId : std::nullopt};
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
    if (body.contains("telegram_chat_id") && body["telegram_chat_id"].is_string()) {
        update.telegramChatId = body["telegram_chat_id"].get<std::string>();
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
        case kTelegramChatIdTaken:
            response.status = 409;
            response.set_content(nlohmann::json{{"error", "telegram_chat_id already in use"}}.dump(),
                                  kJsonContentType);
            return;
        case kUpdated:
            break;
    }
    response.set_content(
        toJson(Profile{.login = *login,
                        .displayName = update.displayName,
                        .avatarUrl = update.avatarUrl,
                        .publicKey = update.publicKey,
                        .email = update.email,
                        .telegramChatId = update.telegramChatId})
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

    const std::optional<OtpIdentity> resolved = userService_.resolveOtpIdentifier(*identifier);
    if (!resolved.has_value()) {
        response.set_content(nlohmann::json{{"found", false}}.dump(), kJsonContentType);
        return;
    }
    response.set_content(nlohmann::json{{"found", true},
                                         {"login", resolved->login},
                                         {"email", resolved->email.has_value() ? nlohmann::json(*resolved->email)
                                                                                : nlohmann::json(nullptr)},
                                         {"telegram_chat_id", resolved->telegramChatId.has_value()
                                                                   ? nlohmann::json(*resolved->telegramChatId)
                                                                   : nlohmann::json(nullptr)}}
                              .dump(),
                          kJsonContentType);
}

void HttpServer::handleSendFriendRequest(const httplib::Request& request, httplib::Response& response) {
    const std::optional<std::string> login = authenticate(request);
    if (!login.has_value()) {
        response.status = 401;
        return;
    }

    const std::optional<std::string> recipientLogin = parseRecipientLogin(request.body);
    if (!recipientLogin.has_value()) {
        response.status = 400;
        response.set_content(nlohmann::json{{"error", "expected a 'recipient_login' string"}}.dump(),
                              kJsonContentType);
        return;
    }

    switch (userService_.sendFriendRequest(*login, *recipientLogin)) {
        using enum SendFriendRequestResult;
        case kCannotFriendSelf:
            response.status = 400;
            response.set_content(nlohmann::json{{"error", "cannot send a friend request to yourself"}}.dump(),
                                  kJsonContentType);
            return;
        case kNoSuchRecipient:
            response.status = 404;
            response.set_content(nlohmann::json{{"error", "no such user"}}.dump(), kJsonContentType);
            return;
        case kAlreadyFriends:
            response.status = 409;
            response.set_content(nlohmann::json{{"error", "already friends"}}.dump(), kJsonContentType);
            return;
        case kAlreadyRequested:
            response.status = 409;
            response.set_content(nlohmann::json{{"error", "a pending request already exists"}}.dump(),
                                  kJsonContentType);
            return;
        case kSent:
            response.status = 201;
            response.set_content(nlohmann::json{{"status", "sent"}}.dump(), kJsonContentType);
            return;
        case kAutoAccepted:
            response.status = 201;
            response.set_content(nlohmann::json{{"status", "accepted"}}.dump(), kJsonContentType);
            return;
    }
}

void HttpServer::handleListIncomingFriendRequests(const httplib::Request& request, httplib::Response& response) {
    const std::optional<std::string> login = authenticate(request);
    if (!login.has_value()) {
        response.status = 401;
        return;
    }

    nlohmann::json requests = nlohmann::json::array();
    for (const FriendRequestInfo& friendRequest : userService_.listIncomingFriendRequests(*login)) {
        requests.push_back(toJson(friendRequest));
    }
    response.set_content(requests.dump(), kJsonContentType);
}

void HttpServer::handleAcceptFriendRequest(const httplib::Request& request, httplib::Response& response) {
    const std::optional<std::string> login = authenticate(request);
    if (!login.has_value()) {
        response.status = 401;
        return;
    }

    const auto requestId = std::stoll(request.matches[1].str());
    switch (userService_.respondToFriendRequest(requestId, *login, /*accept=*/true)) {
        using enum RespondToFriendRequestResult;
        case kNoSuchRequest:
        case kNotYourRequest:
            response.status = 404;
            response.set_content(nlohmann::json{{"error", "no such pending request"}}.dump(), kJsonContentType);
            return;
        case kAccepted:
            response.set_content(nlohmann::json{{"status", "accepted"}}.dump(), kJsonContentType);
            return;
        case kDeclined:
            break;
    }
}

void HttpServer::handleDeclineFriendRequest(const httplib::Request& request, httplib::Response& response) {
    const std::optional<std::string> login = authenticate(request);
    if (!login.has_value()) {
        response.status = 401;
        return;
    }

    const auto requestId = std::stoll(request.matches[1].str());
    switch (userService_.respondToFriendRequest(requestId, *login, /*accept=*/false)) {
        using enum RespondToFriendRequestResult;
        case kNoSuchRequest:
        case kNotYourRequest:
            response.status = 404;
            response.set_content(nlohmann::json{{"error", "no such pending request"}}.dump(), kJsonContentType);
            return;
        case kDeclined:
            response.set_content(nlohmann::json{{"status", "declined"}}.dump(), kJsonContentType);
            return;
        case kAccepted:
            break;
    }
}

void HttpServer::handleListFriends(const httplib::Request& request, httplib::Response& response) {
    const std::optional<std::string> login = authenticate(request);
    if (!login.has_value()) {
        response.status = 401;
        return;
    }

    nlohmann::json logins = nlohmann::json::array();
    for (const std::string& friendLogin : userService_.listFriends(*login)) {
        logins.push_back(friendLogin);
    }
    response.set_content(logins.dump(), kJsonContentType);
}

void HttpServer::handleRemoveFriend(const httplib::Request& request, httplib::Response& response) {
    const std::optional<std::string> login = authenticate(request);
    if (!login.has_value()) {
        response.status = 401;
        return;
    }

    const bool removed = userService_.removeFriend(*login, request.matches[1].str());
    if (!removed) {
        response.status = 404;
        response.set_content(nlohmann::json{{"error", "not friends"}}.dump(), kJsonContentType);
        return;
    }
    response.set_content(nlohmann::json{{"status", "removed"}}.dump(), kJsonContentType);
}

void HttpServer::handleCheckFriendship(const httplib::Request& request, httplib::Response& response) {
    if (!request.has_param("user_a") || !request.has_param("user_b")) {
        response.status = 400;
        response.set_content(nlohmann::json{{"error", "expected 'user_a' and 'user_b' query params"}}.dump(),
                              kJsonContentType);
        return;
    }

    const bool areFriends =
        userService_.areFriends(request.get_param_value("user_a"), request.get_param_value("user_b"));
    response.set_content(nlohmann::json{{"friends", areFriends}}.dump(), kJsonContentType);
}

void HttpServer::listen(const std::string& host, int port) {
    server_.listen(host, port);
}

void HttpServer::stop() {
    server_.stop();
}

}  // namespace user_service
