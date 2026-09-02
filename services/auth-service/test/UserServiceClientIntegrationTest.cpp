#include "UserServiceClient.h"

#include <gtest/gtest.h>
#include <httplib.h>

#include <chrono>
#include <cstdlib>
#include <nlohmann/json.hpp>
#include <string>

// Требует работающего user-service + Postgres (см. services/user-service,
// docker-compose.yml), доступного по USER_SERVICE_HOST/USER_SERVICE_PORT
// (по умолчанию 127.0.0.1:8081). Самоотключается (skip), а не падает,
// когда он не запущен — CI сейчас не оркестрирует оба сервиса вместе,
// так что это ручная/локальная сквозная (end-to-end) проверка.

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

TEST(UserServiceClientIntegrationTest, RegisterUserSucceedsOnceThenRejectsDuplicateLogin) {
    const std::string host = envOrDefault("USER_SERVICE_HOST", "127.0.0.1");
    const int port = std::stoi(envOrDefault("USER_SERVICE_PORT", "8081"));

    const std::string loginPrefix =
        "auth-service-register-test-" +
        std::to_string(
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
                .count());
    const std::string password = "integration-test-password";

    // Подтверждает доступность так же, как это делает
    // VerifiesRegisteredUserAndRejectsWrongPassword: реальная
    // регистрация через "сырой" клиент, с самоотключением (skip), если
    // сервис недоступен, а не с падением теста.
    httplib::Client setupClient(host, port);
    const nlohmann::json probeBody{{"login", loginPrefix + "-probe"}, {"password", password}};
    const httplib::Result probeResult = setupClient.Post("/users/register", probeBody.dump(), "application/json");
    if (!probeResult) {
        GTEST_SKIP() << "user-service not reachable at " << host << ":" << port
                      << " — start docker-compose + user-service locally to run this test.";
    }
    ASSERT_EQ(probeResult->status, 201);

    const UserServiceClient client(host, port);
    const std::string login = loginPrefix + "-wrapper";
    EXPECT_TRUE(client.registerUser(login, password));
    EXPECT_FALSE(client.registerUser(login, password));
}

TEST(UserServiceClientIntegrationTest, VerifyCredentialsRejectsNonexistentLogin) {
    const std::string host = envOrDefault("USER_SERVICE_HOST", "127.0.0.1");
    const int port = std::stoi(envOrDefault("USER_SERVICE_PORT", "8081"));

    httplib::Client probeClient(host, port);
    const nlohmann::json probeBody{{"login", "auth-service-nonexistent-probe"}, {"password", "irrelevant"}};
    const httplib::Result probeResult = probeClient.Post("/users/register", probeBody.dump(), "application/json");
    if (!probeResult) {
        GTEST_SKIP() << "user-service not reachable at " << host << ":" << port
                      << " — start docker-compose + user-service locally to run this test.";
    }

    const UserServiceClient client(host, port);
    EXPECT_FALSE(client.verifyCredentials("no-such-login-ever-registered", "any-password"));
}

TEST(UserServiceClientIntegrationTest, FailsClosedWhenUserServiceIsUnreachable) {
    // Здесь намеренно нет логики самоотключения (skip) — этот тест
    // сознательно нацелен на неиспользуемый loopback-порт, так что
    // результат осмыслен независимо от того, запущен ли где-то ещё
    // реальный user-service.
    const UserServiceClient client("127.0.0.1", 1);

    EXPECT_FALSE(client.verifyCredentials("alice", "password"));
    EXPECT_FALSE(client.registerUser("alice", "password"));
}

}  // namespace
}  // namespace auth_service
