#include "HttpServer.h"

#include <gtest/gtest.h>
#include <httplib.h>

#include <chrono>
#include <cstdlib>
#include <nlohmann/json.hpp>
#include <string>
#include <thread>

#include "TokenService.h"
#include "UserServiceClient.h"

// Тесты уровня маршрутов для HttpServer, обращающиеся к реальному
// экземпляру httplib::Server по loopback-HTTP (а не вызывающие методы
// обработчиков напрямую — они приватные, и суть в том, чтобы проверить
// реальный контракт запрос/ответ). Тестам, которые проверяют только
// валидацию (отсутствующие поля, некорректный JSON) или проверку
// токена (чистая локальная логика в TokenService), не требуется
// запущенный другой сервис. Тестам, которые обращаются к
// UserServiceClient (валидные учётные данные для /auth/token,
// /auth/register), нужен работающий user-service + Postgres, так же
// как и UserServiceClientIntegrationTest, и они самоотключаются
// (skip), а не падают, когда сервис недоступен.

namespace auth_service {
namespace {

std::string envOrDefault(const char* name, const std::string& defaultValue) {
    const char* value = std::getenv(name);
    return value != nullptr ? std::string(value) : defaultValue;
}

constexpr const char* kTestHost = "127.0.0.1";
constexpr int kTestPort = 18080;

// Запускает реальный HttpServer в фоновом потоке на время жизни
// фикстуры и останавливает его при уничтожении, так что каждый TEST
// получает свежий сервер без повторения шаблонного кода запуска потока
// и ожидания готовности.
class ScopedServer {
public:
    ScopedServer(const TokenService& tokenService, const UserServiceClient& userServiceClient)
        : server_(tokenService, userServiceClient),
          thread_([this] { server_.listen(kTestHost, kTestPort); }) {
        httplib::Client probe(kTestHost, kTestPort);
        probe.set_connection_timeout(0, 50000);
        for (int attempt = 0; attempt < 100; ++attempt) {
            if (probe.Get("/")) {
                return;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    }

    ~ScopedServer() {
        server_.stop();
        thread_.join();
    }

    ScopedServer(const ScopedServer&) = delete;
    ScopedServer& operator=(const ScopedServer&) = delete;

private:
    HttpServer server_;
    std::thread thread_;
};

// True, если user-service доступен по USER_SERVICE_HOST/PORT (по
// умолчанию 127.0.0.1:8081), проверяется через одноразовую
// регистрацию, так же как это делает UserServiceClientIntegrationTest.
bool userServiceReachable(const std::string& host, int port) {
    httplib::Client client(host, port);
    const nlohmann::json probeBody{{"login", "http-server-test-probe"}, {"password", "irrelevant"}};
    return static_cast<bool>(client.Post("/users/register", probeBody.dump(), "application/json"));
}

std::string uniqueLogin(const std::string& prefix) {
    return prefix + "-" +
           std::to_string(
               std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
                   .count());
}

TEST(HttpServerTest, TokenRouteRejectsMissingFieldsWith400) {
    const TokenService tokenService("test-secret");
    const UserServiceClient userServiceClient("127.0.0.1", 1);  // недоступен, не должен вызываться
    const ScopedServer server(tokenService, userServiceClient);

    httplib::Client client(kTestHost, kTestPort);
    const httplib::Result result = client.Post("/auth/token", nlohmann::json{{"login", "alice"}}.dump(), "application/json");

    ASSERT_TRUE(result);
    EXPECT_EQ(result->status, 400);
}

TEST(HttpServerTest, TokenRouteRejectsMalformedJsonWith400) {
    const TokenService tokenService("test-secret");
    const UserServiceClient userServiceClient("127.0.0.1", 1);
    const ScopedServer server(tokenService, userServiceClient);

    httplib::Client client(kTestHost, kTestPort);
    const httplib::Result result = client.Post("/auth/token", "not json", "application/json");

    ASSERT_TRUE(result);
    EXPECT_EQ(result->status, 400);
}

TEST(HttpServerTest, TokenRouteIssuesTokenForValidCredentials) {
    const std::string userServiceHost = envOrDefault("USER_SERVICE_HOST", "127.0.0.1");
    const int userServicePort = std::stoi(envOrDefault("USER_SERVICE_PORT", "8081"));
    if (!userServiceReachable(userServiceHost, userServicePort)) {
        GTEST_SKIP() << "user-service not reachable at " << userServiceHost << ":" << userServicePort;
    }

    const std::string login = uniqueLogin("http-server-token-test");
    const std::string password = "integration-test-password";
    httplib::Client userServiceSetup(userServiceHost, userServicePort);
    ASSERT_TRUE(userServiceSetup.Post("/users/register", nlohmann::json{{"login", login}, {"password", password}}.dump(),
                                       "application/json"));

    const TokenService tokenService("test-secret");
    const UserServiceClient userServiceClient(userServiceHost, userServicePort);
    const ScopedServer server(tokenService, userServiceClient);

    httplib::Client client(kTestHost, kTestPort);
    const httplib::Result result =
        client.Post("/auth/token", nlohmann::json{{"login", login}, {"password", password}}.dump(), "application/json");

    ASSERT_TRUE(result);
    ASSERT_EQ(result->status, 200);
    const nlohmann::json body = nlohmann::json::parse(result->body);
    EXPECT_FALSE(body["token"].get<std::string>().empty());
    EXPECT_GT(body["expires_at"].get<std::int64_t>(), 0);
}

TEST(HttpServerTest, TokenRouteRejectsInvalidCredentialsWith401) {
    const std::string userServiceHost = envOrDefault("USER_SERVICE_HOST", "127.0.0.1");
    const int userServicePort = std::stoi(envOrDefault("USER_SERVICE_PORT", "8081"));
    if (!userServiceReachable(userServiceHost, userServicePort)) {
        GTEST_SKIP() << "user-service not reachable at " << userServiceHost << ":" << userServicePort;
    }

    const std::string login = uniqueLogin("http-server-token-bad-test");
    httplib::Client userServiceSetup(userServiceHost, userServicePort);
    ASSERT_TRUE(userServiceSetup.Post(
        "/users/register", nlohmann::json{{"login", login}, {"password", "correct-password"}}.dump(), "application/json"));

    const TokenService tokenService("test-secret");
    const UserServiceClient userServiceClient(userServiceHost, userServicePort);
    const ScopedServer server(tokenService, userServiceClient);

    httplib::Client client(kTestHost, kTestPort);
    const httplib::Result result = client.Post(
        "/auth/token", nlohmann::json{{"login", login}, {"password", "wrong-password"}}.dump(), "application/json");

    ASSERT_TRUE(result);
    EXPECT_EQ(result->status, 401);
}

TEST(HttpServerTest, VerifyRouteRejectsMissingFieldWith400) {
    const TokenService tokenService("test-secret");
    const UserServiceClient userServiceClient("127.0.0.1", 1);
    const ScopedServer server(tokenService, userServiceClient);

    httplib::Client client(kTestHost, kTestPort);
    const httplib::Result result = client.Post("/auth/verify", nlohmann::json::object().dump(), "application/json");

    ASSERT_TRUE(result);
    EXPECT_EQ(result->status, 400);
}

TEST(HttpServerTest, VerifyRouteRejectsMalformedJsonWith400) {
    const TokenService tokenService("test-secret");
    const UserServiceClient userServiceClient("127.0.0.1", 1);
    const ScopedServer server(tokenService, userServiceClient);

    httplib::Client client(kTestHost, kTestPort);
    const httplib::Result result = client.Post("/auth/verify", "not json", "application/json");

    ASSERT_TRUE(result);
    EXPECT_EQ(result->status, 400);
}

TEST(HttpServerTest, VerifyRouteReturnsInvalidForBadToken) {
    const TokenService tokenService("test-secret");
    const UserServiceClient userServiceClient("127.0.0.1", 1);
    const ScopedServer server(tokenService, userServiceClient);

    httplib::Client client(kTestHost, kTestPort);
    const httplib::Result result =
        client.Post("/auth/verify", nlohmann::json{{"token", "not-a-real-token"}}.dump(), "application/json");

    ASSERT_TRUE(result);
    ASSERT_EQ(result->status, 200);
    const nlohmann::json body = nlohmann::json::parse(result->body);
    EXPECT_FALSE(body["valid"].get<bool>());
}

TEST(HttpServerTest, VerifyRouteReturnsValidForGoodToken) {
    const TokenService tokenService("test-secret");
    const UserServiceClient userServiceClient("127.0.0.1", 1);
    const Token token = tokenService.issueToken("alice");
    const ScopedServer server(tokenService, userServiceClient);

    httplib::Client client(kTestHost, kTestPort);
    const httplib::Result result = client.Post("/auth/verify", nlohmann::json{{"token", token.value}}.dump(), "application/json");

    ASSERT_TRUE(result);
    ASSERT_EQ(result->status, 200);
    const nlohmann::json body = nlohmann::json::parse(result->body);
    EXPECT_TRUE(body["valid"].get<bool>());
    EXPECT_EQ(body["subject"].get<std::string>(), "alice");
}

TEST(HttpServerTest, RegisterRouteRejectsMissingFieldsWith400) {
    const TokenService tokenService("test-secret");
    const UserServiceClient userServiceClient("127.0.0.1", 1);
    const ScopedServer server(tokenService, userServiceClient);

    httplib::Client client(kTestHost, kTestPort);
    const httplib::Result result =
        client.Post("/auth/register", nlohmann::json{{"login", "alice"}}.dump(), "application/json");

    ASSERT_TRUE(result);
    EXPECT_EQ(result->status, 400);
}

TEST(HttpServerTest, RegisterRouteRejectsMalformedJsonWith400) {
    const TokenService tokenService("test-secret");
    const UserServiceClient userServiceClient("127.0.0.1", 1);
    const ScopedServer server(tokenService, userServiceClient);

    httplib::Client client(kTestHost, kTestPort);
    const httplib::Result result = client.Post("/auth/register", "not json", "application/json");

    ASSERT_TRUE(result);
    EXPECT_EQ(result->status, 400);
}

TEST(HttpServerTest, RegisterRouteCreatesAccountAndAutoIssuesToken) {
    const std::string userServiceHost = envOrDefault("USER_SERVICE_HOST", "127.0.0.1");
    const int userServicePort = std::stoi(envOrDefault("USER_SERVICE_PORT", "8081"));
    if (!userServiceReachable(userServiceHost, userServicePort)) {
        GTEST_SKIP() << "user-service not reachable at " << userServiceHost << ":" << userServicePort;
    }

    const TokenService tokenService("test-secret");
    const UserServiceClient userServiceClient(userServiceHost, userServicePort);
    const ScopedServer server(tokenService, userServiceClient);

    const std::string login = uniqueLogin("http-server-register-test");
    httplib::Client client(kTestHost, kTestPort);
    const httplib::Result result = client.Post(
        "/auth/register", nlohmann::json{{"login", login}, {"password", "integration-test-password"}}.dump(),
        "application/json");

    ASSERT_TRUE(result);
    ASSERT_EQ(result->status, 201);
    const nlohmann::json body = nlohmann::json::parse(result->body);
    EXPECT_TRUE(body["registered"].get<bool>());
    EXPECT_FALSE(body["token"].get<std::string>().empty());
    EXPECT_GT(body["expires_at"].get<std::int64_t>(), 0);
}

TEST(HttpServerTest, RegisterRouteRejectsDuplicateLoginWith409) {
    const std::string userServiceHost = envOrDefault("USER_SERVICE_HOST", "127.0.0.1");
    const int userServicePort = std::stoi(envOrDefault("USER_SERVICE_PORT", "8081"));
    if (!userServiceReachable(userServiceHost, userServicePort)) {
        GTEST_SKIP() << "user-service not reachable at " << userServiceHost << ":" << userServicePort;
    }

    const std::string login = uniqueLogin("http-server-register-dup-test");
    httplib::Client userServiceSetup(userServiceHost, userServicePort);
    ASSERT_TRUE(userServiceSetup.Post(
        "/users/register", nlohmann::json{{"login", login}, {"password", "integration-test-password"}}.dump(),
        "application/json"));

    const TokenService tokenService("test-secret");
    const UserServiceClient userServiceClient(userServiceHost, userServicePort);
    const ScopedServer server(tokenService, userServiceClient);

    httplib::Client client(kTestHost, kTestPort);
    const httplib::Result result = client.Post(
        "/auth/register", nlohmann::json{{"login", login}, {"password", "integration-test-password"}}.dump(),
        "application/json");

    ASSERT_TRUE(result);
    ASSERT_EQ(result->status, 409);
    const nlohmann::json body = nlohmann::json::parse(result->body);
    EXPECT_FALSE(body["registered"].get<bool>());
}

}  // namespace
}  // namespace auth_service
