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

}  // namespace chat_service
