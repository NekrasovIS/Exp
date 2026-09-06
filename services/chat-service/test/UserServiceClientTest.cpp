#include "UserServiceClient.h"

#include <gtest/gtest.h>
#include <httplib.h>

#include <chrono>
#include <cstdlib>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>

// Требует работающий auth-service (для выпуска реальных токенов) И
// работающий user-service (для проверки самой дружбы) — та же пара
// зависимостей, что и у HttpServerTest.cpp в user-service. Пропускает
// себя, а не падает, если что-то из этого недоступно. Случай
// недоступного user-service не требует ни того, ни другого и
// выполняется всегда.

namespace chat_service {
namespace {

std::string envOrDefault(const char* name, const std::string& defaultValue) {
    const char* value = std::getenv(name);
    return value != nullptr ? std::string(value) : defaultValue;
}

std::string uniqueLogin(const std::string& prefix) {
    return prefix + "-" +
           std::to_string(
               std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
                   .count());
}

struct TestAccount {
    std::string login;
    std::string token;
};

// Регистрирует через auth-service (не напрямую user-service) — так
// вызывающая сторона сразу получает настоящий Bearer-токен, которым
// можно вызвать POST /friends/requests на user-service.
std::optional<TestAccount> registerViaAuthService(const std::string& loginPrefix) {
    const std::string authHost = envOrDefault("AUTH_SERVICE_HOST", "127.0.0.1");
    const int authPort = std::stoi(envOrDefault("AUTH_SERVICE_PORT", "8080"));
    httplib::Client client(authHost, authPort);
    const std::string login = uniqueLogin(loginPrefix);
    const nlohmann::json body{{"login", login}, {"password", "user-service-client-test-password"}};
    const httplib::Result result = client.Post("/auth/register", body.dump(), "application/json");
    if (!result || result->status != 201) {
        return std::nullopt;
    }
    const nlohmann::json response = nlohmann::json::parse(result->body, nullptr, /*allow_exceptions=*/false);
    if (response.is_discarded() || !response.contains("token")) {
        return std::nullopt;
    }
    return TestAccount{.login = login, .token = response["token"].get<std::string>()};
}

std::pair<std::string, int> userServiceHostAndPort() {
    return {envOrDefault("USER_SERVICE_HOST", "127.0.0.1"), std::stoi(envOrDefault("USER_SERVICE_PORT", "8081"))};
}

TEST(UserServiceClientTest, AreFriendsReflectsWhetherTheTwoAccountsAreFriends) {
    const std::optional<TestAccount> accountA = registerViaAuthService("user-service-client-friends-a");
    const std::optional<TestAccount> accountB = registerViaAuthService("user-service-client-friends-b");
    if (!accountA.has_value() || !accountB.has_value()) {
        GTEST_SKIP() << "auth-service (and the user-service it forwards to) not reachable — start the full stack.";
    }

    const auto [userServiceHost, userServicePort] = userServiceHostAndPort();
    httplib::Client userServiceClient(userServiceHost, userServicePort);
    httplib::Headers authHeaderA{{"Authorization", "Bearer " + accountA->token}};
    httplib::Headers authHeaderB{{"Authorization", "Bearer " + accountB->token}};

    const UserServiceClient client(userServiceHost, userServicePort);
    EXPECT_FALSE(client.areFriends(accountA->login, accountB->login));

    // Взаимная заявка (issue #187, Фаза 1) — сразу дружба.
    ASSERT_TRUE(userServiceClient.Post("/friends/requests", authHeaderA,
                                        nlohmann::json{{"recipient_login", accountB->login}}.dump(),
                                        "application/json"));
    ASSERT_TRUE(userServiceClient.Post("/friends/requests", authHeaderB,
                                        nlohmann::json{{"recipient_login", accountA->login}}.dump(),
                                        "application/json"));

    EXPECT_TRUE(client.areFriends(accountA->login, accountB->login));
    EXPECT_TRUE(client.areFriends(accountB->login, accountA->login));
}

TEST(UserServiceClientTest, AreFriendsFailsClosedWhenUserServiceIsUnreachable) {
    // Намеренно нет логики пропуска — цель тут неиспользуемый loopback-порт.
    const UserServiceClient client("127.0.0.1", 1);

    EXPECT_FALSE(client.areFriends("anyone", "someone-else"));
}

}  // namespace
}  // namespace chat_service
