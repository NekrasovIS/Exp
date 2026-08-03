#include "UserServiceClient.h"

#include <gtest/gtest.h>
#include <httplib.h>

#include <chrono>
#include <cstdlib>
#include <nlohmann/json.hpp>
#include <string>

// Requires a live user-service + Postgres (see services/user-service,
// docker-compose.yml) reachable at USER_SERVICE_HOST/USER_SERVICE_PORT
// (defaults 127.0.0.1:8081). Skips itself rather than failing when it
// isn't running — CI doesn't currently orchestrate both services
// together, so this is a manual/local end-to-end check.

namespace auth_service {
namespace {

std::string envOrDefault(const char* name, const std::string& defaultValue) {
    const char* value = std::getenv(name);
    return value != nullptr ? std::string(value) : defaultValue;
}

TEST(UserServiceClientIntegrationTest, VerifiesRegisteredUserAndRejectsWrongPassword) {
    const std::string host = envOrDefault("USER_SERVICE_HOST", "127.0.0.1");
    const int port = std::stoi(envOrDefault("USER_SERVICE_PORT", "8081"));

    const std::string login =
        "auth-service-integration-test-" +
        std::to_string(
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
                .count());
    const std::string password = "integration-test-password";

    httplib::Client setupClient(host, port);
    const nlohmann::json registerBody{{"login", login}, {"password", password}};
    const httplib::Result registerResult =
        setupClient.Post("/users/register", registerBody.dump(), "application/json");

    if (!registerResult) {
        GTEST_SKIP() << "user-service not reachable at " << host << ":" << port
                      << " — start docker-compose + user-service locally to run this test.";
    }
    ASSERT_EQ(registerResult->status, 201);

    const UserServiceClient client(host, port);
    EXPECT_TRUE(client.verifyCredentials(login, password));
    EXPECT_FALSE(client.verifyCredentials(login, "wrong-password"));
}

}  // namespace
}  // namespace auth_service
