#include "UserRepository.h"

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

std::string connectionString() {
    return envOrDefault("USER_SERVICE_DATABASE_URL",
                         "postgresql://user_service:dev-only-password@localhost:5433/user_service");
}

std::string uniqueLogin(const std::string& prefix) {
    return prefix + "-" +
           std::to_string(
               std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
                   .count());
}

TEST(UserRepositoryTest, CreateUserSucceedsForFreshLogin) {
    UserRepository repository(connectionString());
    const std::string login = uniqueLogin("user-repository-test-fresh");

    bool created = false;
    try {
        created = repository.createUser(login, "some-hash");
    } catch (const std::exception& error) {
        GTEST_SKIP() << "Postgres not reachable (" << error.what() << ") — run `docker compose up` to run this test.";
    }

    EXPECT_TRUE(created);
}

TEST(UserRepositoryTest, CreateUserRejectsDuplicateLogin) {
    UserRepository repository(connectionString());
    const std::string login = uniqueLogin("user-repository-test-dup");

    bool firstCreated = false;
    try {
        firstCreated = repository.createUser(login, "some-hash");
    } catch (const std::exception& error) {
        GTEST_SKIP() << "Postgres not reachable (" << error.what() << ") — run `docker compose up` to run this test.";
    }
    ASSERT_TRUE(firstCreated);

    EXPECT_FALSE(repository.createUser(login, "a-different-hash"));
}

TEST(UserRepositoryTest, FindPasswordHashReturnsStoredHashForExistingLogin) {
    UserRepository repository(connectionString());
    const std::string login = uniqueLogin("user-repository-test-find");

    bool created = false;
    try {
        created = repository.createUser(login, "the-stored-hash");
    } catch (const std::exception& error) {
        GTEST_SKIP() << "Postgres not reachable (" << error.what() << ") — run `docker compose up` to run this test.";
    }
    ASSERT_TRUE(created);

    const std::optional<std::string> hash = repository.findPasswordHash(login);

    ASSERT_TRUE(hash.has_value());
    EXPECT_EQ(*hash, "the-stored-hash");
}

TEST(UserRepositoryTest, FindPasswordHashReturnsNulloptForNonexistentLogin) {
    UserRepository repository(connectionString());

    std::optional<std::string> hash;
    try {
        hash = repository.findPasswordHash("no-such-login-ever-created");
    } catch (const std::exception& error) {
        GTEST_SKIP() << "Postgres not reachable (" << error.what() << ") — run `docker compose up` to run this test.";
    }

    EXPECT_FALSE(hash.has_value());
}

}  // namespace
}  // namespace user_service
