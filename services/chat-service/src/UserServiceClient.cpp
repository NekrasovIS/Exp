#include "UserServiceClient.h"

#include <httplib.h>

#include <nlohmann/json.hpp>

namespace chat_service {

UserServiceClient::UserServiceClient(std::string host, int port) : host_(std::move(host)), port_(port) {}

bool UserServiceClient::areFriends(const std::string& loginA, const std::string& loginB) const {
    httplib::Client client(host_, port_);

    const httplib::Params params{{"user_a", loginA}, {"user_b", loginB}};
    const httplib::Result result = client.Get("/internal/friendship", params);

    if (!result || result->status != 200) {
        return false;
    }

    const nlohmann::json response = nlohmann::json::parse(result->body, nullptr, /*allow_exceptions=*/false);
    if (response.is_discarded()) {
        return false;
    }
    return response.value("friends", false);
}

bool UserServiceClient::isBlocked(const std::string& blockerLogin, const std::string& blockedLogin) const {
    httplib::Client client(host_, port_);

    const httplib::Params params{{"blocker", blockerLogin}, {"blocked", blockedLogin}};
    const httplib::Result result = client.Get("/internal/blocked", params);

    if (!result || result->status != 200) {
        // Fail closed — см. doc-комментарий isBlocked() в заголовке.
        return true;
    }

    const nlohmann::json response = nlohmann::json::parse(result->body, nullptr, /*allow_exceptions=*/false);
    if (response.is_discarded()) {
        return true;
    }
    return response.value("blocked", true);
}

}  // namespace chat_service
