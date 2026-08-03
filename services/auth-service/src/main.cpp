#include <cstdlib>
#include <iostream>
#include <string>

#include "HttpServer.h"
#include "TokenService.h"
#include "UserServiceClient.h"

namespace {

std::string envOrDefault(const char* name, const std::string& defaultValue) {
    const char* value = std::getenv(name);
    return value != nullptr ? std::string(value) : defaultValue;
}

}  // namespace

int main() {
    const char* secret = std::getenv("AUTH_SERVICE_SECRET");
    if (secret == nullptr || std::string(secret).empty()) {
        std::cerr << "AUTH_SERVICE_SECRET environment variable is required (and must be non-empty) — "
                      "refusing to start with no signing key.\n";
        return 1;
    }

    const std::string host = envOrDefault("AUTH_SERVICE_HOST", "127.0.0.1");
    const int port = std::stoi(envOrDefault("AUTH_SERVICE_PORT", "8080"));
    const std::string userServiceHost = envOrDefault("USER_SERVICE_HOST", "127.0.0.1");
    const int userServicePort = std::stoi(envOrDefault("USER_SERVICE_PORT", "8081"));

    const auth_service::TokenService tokenService(secret);
    const auth_service::UserServiceClient userServiceClient(userServiceHost, userServicePort);
    auth_service::HttpServer server(tokenService, userServiceClient);

    std::cout << "auth-service listening on " << host << ":" << port << "\n";
    server.listen(host, port);

    return 0;
}
