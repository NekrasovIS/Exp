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

}  // namespace auth_service
