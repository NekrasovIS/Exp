#include "UserServiceClient.h"

#include <httplib.h>

#include <nlohmann/json.hpp>

namespace auth_service {

UserServiceClient::UserServiceClient(std::string host, int port) : host_(std::move(host)), port_(port) {}

bool UserServiceClient::verifyCredentials(const std::string& login, const std::string& password) const {
    httplib::Client client(host_, port_);

    const nlohmann::json body{{"login", login}, {"password", password}};
    const httplib::Result result = client.Post("/users/verify-credentials", body.dump(), "application/json");

    if (!result || result->status != 200) {
        return false;
    }

    const nlohmann::json response = nlohmann::json::parse(result->body, nullptr, /*allow_exceptions=*/false);
    return !response.is_discarded() && response.value("valid", false);
}

bool UserServiceClient::registerUser(const std::string& login, const std::string& password) const {
    httplib::Client client(host_, port_);

    const nlohmann::json body{{"login", login}, {"password", password}};
    const httplib::Result result = client.Post("/users/register", body.dump(), "application/json");

    return result && result->status == 201;
}

std::optional<OtpIdentity> UserServiceClient::resolveOtpIdentifier(const std::string& identifier) const {
    httplib::Client client(host_, port_);

    const nlohmann::json body{{"identifier", identifier}};
    const httplib::Result result = client.Post("/users/resolve-otp-identifier", body.dump(), "application/json");
    if (!result || result->status != 200) {
        return std::nullopt;
    }

    const nlohmann::json response = nlohmann::json::parse(result->body, nullptr, /*allow_exceptions=*/false);
    if (response.is_discarded() || !response.value("found", false)) {
        return std::nullopt;
    }
    return OtpIdentity{
        .login = response.value("login", std::string()),
        .email = response.contains("email") && response["email"].is_string()
                     ? std::make_optional(response["email"].get<std::string>())
                     : std::nullopt,
        .telegramChatId = response.contains("telegram_chat_id") && response["telegram_chat_id"].is_string()
                               ? std::make_optional(response["telegram_chat_id"].get<std::string>())
                               : std::nullopt};
}

}  // namespace auth_service
