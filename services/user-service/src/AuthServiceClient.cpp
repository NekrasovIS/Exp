#include "AuthServiceClient.h"

#include <httplib.h>

#include <nlohmann/json.hpp>

namespace user_service {

AuthServiceClient::AuthServiceClient(std::string host, int port) : host_(std::move(host)), port_(port) {}

std::optional<std::string> AuthServiceClient::verifyToken(const std::string& token) const {
    httplib::Client client(host_, port_);

    const nlohmann::json body{{"token", token}};
    const httplib::Result result = client.Post("/auth/verify", body.dump(), "application/json");

    if (!result || result->status != 200) {
        return std::nullopt;
    }

    const nlohmann::json response = nlohmann::json::parse(result->body, nullptr, /*allow_exceptions=*/false);
    if (response.is_discarded() || !response.value("valid", false)) {
        return std::nullopt;
    }

    return response.value("subject", std::string());
}

}  // namespace user_service
