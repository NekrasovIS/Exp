#include <cstdlib>
#include <iostream>
#include <string>

#include "HttpServer.h"
#include "UserRepository.h"
#include "UserService.h"

namespace {

std::string envOrDefault(const char* name, const std::string& defaultValue) {
    const char* value = std::getenv(name);
    return value != nullptr ? std::string(value) : defaultValue;
}

}  // namespace

int main() {
    // Matches docker-compose.yml's default local Postgres — override for
    // anything beyond a throwaway local instance.
    const std::string connectionString = envOrDefault(
        "USER_SERVICE_DATABASE_URL", "postgresql://user_service:dev-only-password@localhost:5432/user_service");
    const std::string host = envOrDefault("USER_SERVICE_HOST", "127.0.0.1");
    const int port = std::stoi(envOrDefault("USER_SERVICE_PORT", "8081"));

    user_service::UserRepository repository(connectionString);
    user_service::UserService service(repository);
    user_service::HttpServer server(service);

    std::cout << "user-service listening on " << host << ":" << port << "\n";
    server.listen(host, port);

    return 0;
}
