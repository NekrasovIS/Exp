#include "UserRepository.h"
#include "UserService.h"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdlib>
#include <stdexcept>
#include <string>

// Requires a live Postgres (see docker-compose.yml) reachable at
// USER_SERVICE_DATABASE_URL (default matches docker-compose.yml).
// Skips itself rather than failing when it isn't running.

namespace user_service {
namespace {

std::string envOrDefault(const char* name, const std::string& defaultValue) {
    const char* value = std::getenv(name);
    return value != nullptr ? std::string(value) : defaultValue;
}

TEST(UserServiceIntegrationTest, RegisterThenVerifyCredentialsRoundTrip) {
    const std::string connectionString = envOrDefault(
        "USER_SERVICE_DATABASE_URL", "postgresql://user_service:dev-only-password@localhost:5432/user_service");

    UserRepository repository(connectionString);
    UserService service(repository);

    const std::string login =
        "user-service-integration-test-" +
        std::to_string(
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
                .count());
    const std::string password = "integration-test-password";

    bool registered = false;
    try {
        registered = service.registerUser(login, password);
    } catch (const std::exception& error) {
        GTEST_SKIP() << "Postgres not reachable (" << error.what() << ") — run `docker compose up` to run this test.";
    }

    ASSERT_TRUE(registered);
    EXPECT_TRUE(service.verifyCredentials(login, password));
    EXPECT_FALSE(service.verifyCredentials(login, "wrong-password"));
    EXPECT_FALSE(service.verifyCredentials("no-such-user", password));
}

}  // namespace
}  // namespace user_service
