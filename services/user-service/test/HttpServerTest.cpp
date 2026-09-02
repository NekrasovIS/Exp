#include "HttpServer.h"

#include <gtest/gtest.h>
#include <httplib.h>

#include <chrono>
#include <cstdlib>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <thread>

#include "AuthServiceClient.h"
#include "UserRepository.h"
#include "UserService.h"

// Тесты уровня маршрутов для HttpServer, обращающиеся к реальному экземпляру
// httplib::Server через loopback HTTP. Требуют работающий Postgres (см.
// docker-compose.yml), доступный по USER_SERVICE_DATABASE_URL, так же как
// UserServiceIntegrationTest/UserRepositoryTest — пропускают себя вместо
// падения, если он не запущен.

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

constexpr const char* kTestHost = "127.0.0.1";
constexpr int kTestPort = 18081;

// Реальный AuthServiceClient, указывающий на тот auth-service, что настроен
// для этого запуска тестов (те же переменные окружения, что и в
// AuthServiceClientTest.cpp) — неаутентифицированные маршруты
// (register/verify-credentials) никогда его не вызывают, поэтому его
// безопасно создавать, даже если auth-service недоступен.
AuthServiceClient testAuthServiceClient() {
    return AuthServiceClient(envOrDefault("AUTH_SERVICE_HOST", "127.0.0.1"),
                              std::stoi(envOrDefault("AUTH_SERVICE_PORT", "8080")));
}

// Запускает реальный HttpServer в фоновом потоке на время жизни фикстуры
// и останавливает его в деструкторе.
class ScopedServer {
public:
    ScopedServer(UserService& userService, const AuthServiceClient& authServiceClient)
        : server_(userService, authServiceClient), thread_([this] { server_.listen(kTestHost, kTestPort); }) {
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

TEST(HttpServerTest, RegisterRouteRejectsMissingFieldsWith400) {
    UserRepository repository(connectionString());
    UserService userService(repository);
    const AuthServiceClient authServiceClient = testAuthServiceClient();
    const ScopedServer server(userService, authServiceClient);

    httplib::Client client(kTestHost, kTestPort);
    const httplib::Result result =
        client.Post("/users/register", nlohmann::json{{"login", "alice"}}.dump(), "application/json");

    ASSERT_TRUE(result);
    EXPECT_EQ(result->status, 400);
}

TEST(HttpServerTest, RegisterRouteRejectsMalformedJsonWith400) {
    UserRepository repository(connectionString());
    UserService userService(repository);
    const AuthServiceClient authServiceClient = testAuthServiceClient();
    const ScopedServer server(userService, authServiceClient);

    httplib::Client client(kTestHost, kTestPort);
    const httplib::Result result = client.Post("/users/register", "not json", "application/json");

    ASSERT_TRUE(result);
    EXPECT_EQ(result->status, 400);
}

TEST(HttpServerTest, RegisterRouteCreatesAccountWith201) {
    UserRepository repository(connectionString());
    UserService userService(repository);

    const std::string login = uniqueLogin("http-server-register-test");
    // Проверка доступности: регистрация одноразового логина ниже выбросит
    // исключение (а не просто вернёт неудачу), если Postgres не поднят,
    // поскольку UserRepository открывает реальное соединение на каждый вызов.
    try {
        static_cast<void>(userService.registerUser("http-server-test-reachability-probe", "irrelevant"));
    } catch (const std::exception& error) {
        GTEST_SKIP() << "Postgres not reachable (" << error.what() << ") — run `docker compose up` to run this test.";
    }

    const AuthServiceClient authServiceClient = testAuthServiceClient();
    const ScopedServer server(userService, authServiceClient);
    httplib::Client client(kTestHost, kTestPort);
    const httplib::Result result = client.Post(
        "/users/register", nlohmann::json{{"login", login}, {"password", "integration-test-password"}}.dump(),
        "application/json");

    ASSERT_TRUE(result);
    ASSERT_EQ(result->status, 201);
    const nlohmann::json body = nlohmann::json::parse(result->body);
    EXPECT_TRUE(body["registered"].get<bool>());
}

TEST(HttpServerTest, RegisterRouteRejectsDuplicateLoginWith409) {
    UserRepository repository(connectionString());
    UserService userService(repository);

    const std::string login = uniqueLogin("http-server-register-dup-test");
    bool firstRegistered = false;
    try {
        firstRegistered = userService.registerUser(login, "integration-test-password");
    } catch (const std::exception& error) {
        GTEST_SKIP() << "Postgres not reachable (" << error.what() << ") — run `docker compose up` to run this test.";
    }
    ASSERT_TRUE(firstRegistered);

    const AuthServiceClient authServiceClient = testAuthServiceClient();
    const ScopedServer server(userService, authServiceClient);
    httplib::Client client(kTestHost, kTestPort);
    const httplib::Result result = client.Post(
        "/users/register", nlohmann::json{{"login", login}, {"password", "integration-test-password"}}.dump(),
        "application/json");

    ASSERT_TRUE(result);
    ASSERT_EQ(result->status, 409);
    const nlohmann::json body = nlohmann::json::parse(result->body);
    EXPECT_FALSE(body["registered"].get<bool>());
}

TEST(HttpServerTest, VerifyCredentialsRouteRejectsMissingFieldsWith400) {
    UserRepository repository(connectionString());
    UserService userService(repository);
    const AuthServiceClient authServiceClient = testAuthServiceClient();
    const ScopedServer server(userService, authServiceClient);

    httplib::Client client(kTestHost, kTestPort);
    const httplib::Result result =
        client.Post("/users/verify-credentials", nlohmann::json{{"login", "alice"}}.dump(), "application/json");

    ASSERT_TRUE(result);
    EXPECT_EQ(result->status, 400);
}

TEST(HttpServerTest, VerifyCredentialsRouteRejectsMalformedJsonWith400) {
    UserRepository repository(connectionString());
    UserService userService(repository);
    const AuthServiceClient authServiceClient = testAuthServiceClient();
    const ScopedServer server(userService, authServiceClient);

    httplib::Client client(kTestHost, kTestPort);
    const httplib::Result result = client.Post("/users/verify-credentials", "not json", "application/json");

    ASSERT_TRUE(result);
    EXPECT_EQ(result->status, 400);
}

TEST(HttpServerTest, VerifyCredentialsRouteReturnsTrueForCorrectPassword) {
    UserRepository repository(connectionString());
    UserService userService(repository);

    const std::string login = uniqueLogin("http-server-verify-test");
    const std::string password = "integration-test-password";
    bool registered = false;
    try {
        registered = userService.registerUser(login, password);
    } catch (const std::exception& error) {
        GTEST_SKIP() << "Postgres not reachable (" << error.what() << ") — run `docker compose up` to run this test.";
    }
    ASSERT_TRUE(registered);

    const AuthServiceClient authServiceClient = testAuthServiceClient();
    const ScopedServer server(userService, authServiceClient);
    httplib::Client client(kTestHost, kTestPort);
    const httplib::Result result = client.Post(
        "/users/verify-credentials", nlohmann::json{{"login", login}, {"password", password}}.dump(), "application/json");

    ASSERT_TRUE(result);
    ASSERT_EQ(result->status, 200);
    const nlohmann::json body = nlohmann::json::parse(result->body);
    EXPECT_TRUE(body["valid"].get<bool>());
}

TEST(HttpServerTest, VerifyCredentialsRouteReturnsFalseForWrongPassword) {
    UserRepository repository(connectionString());
    UserService userService(repository);

    const std::string login = uniqueLogin("http-server-verify-wrong-test");
    bool registered = false;
    try {
        registered = userService.registerUser(login, "correct-password");
    } catch (const std::exception& error) {
        GTEST_SKIP() << "Postgres not reachable (" << error.what() << ") — run `docker compose up` to run this test.";
    }
    ASSERT_TRUE(registered);

    const AuthServiceClient authServiceClient = testAuthServiceClient();
    const ScopedServer server(userService, authServiceClient);
    httplib::Client client(kTestHost, kTestPort);
    const httplib::Result result = client.Post(
        "/users/verify-credentials", nlohmann::json{{"login", login}, {"password", "wrong-password"}}.dump(),
        "application/json");

    ASSERT_TRUE(result);
    ASSERT_EQ(result->status, 200);
    const nlohmann::json body = nlohmann::json::parse(result->body);
    EXPECT_FALSE(body["valid"].get<bool>());
}

// Регистрирует совершенно нового пользователя напрямую в живом auth-service
// (не в ScopedServer этого теста) и возвращает автоматически выданный токен —
// маршрутам профиля нужен токен, который auth-service реально сможет
// проверить. Пустая строка означает, что auth-service недоступен.
std::string registerViaAuthServiceAndGetToken(const std::string& loginPrefix) {
    const std::string authHost = envOrDefault("AUTH_SERVICE_HOST", "127.0.0.1");
    const int authPort = std::stoi(envOrDefault("AUTH_SERVICE_PORT", "8080"));
    httplib::Client client(authHost, authPort);
    const nlohmann::json body{{"login", uniqueLogin(loginPrefix)}, {"password", "http-server-profile-test-password"}};
    const httplib::Result result = client.Post("/auth/register", body.dump(), "application/json");
    if (!result || result->status != 201) {
        return {};
    }
    const nlohmann::json response = nlohmann::json::parse(result->body, nullptr, /*allow_exceptions=*/false);
    if (response.is_discarded() || !response.contains("token")) {
        return {};
    }
    return response["token"].get<std::string>();
}

TEST(HttpServerTest, GetProfileRouteRejectsMissingTokenWith401) {
    UserRepository repository(connectionString());
    UserService userService(repository);
    const AuthServiceClient authServiceClient = testAuthServiceClient();
    const ScopedServer server(userService, authServiceClient);

    httplib::Client client(kTestHost, kTestPort);
    const httplib::Result result = client.Get("/users/anyone/profile");

    ASSERT_TRUE(result);
    EXPECT_EQ(result->status, 401);
}

TEST(HttpServerTest, UpdateOwnProfileRoundTripsThroughGetProfileAndPreservesUnsetFieldsOnPartialUpdate) {
    const std::string token = registerViaAuthServiceAndGetToken("http-server-profile-test");
    if (token.empty()) {
        GTEST_SKIP() << "auth-service (and the user-service it forwards to) not reachable — start the full stack.";
    }

    UserRepository repository(connectionString());
    UserService userService(repository);
    const AuthServiceClient authServiceClient = testAuthServiceClient();
    const ScopedServer server(userService, authServiceClient);

    httplib::Client client(kTestHost, kTestPort);
    httplib::Headers authHeader{{"Authorization", "Bearer " + token}};

    // Полное обновление: все три поля.
    const httplib::Result patchResult = client.Patch(
        "/users/me", authHeader,
        nlohmann::json{{"display_name", "Alice"},
                       {"avatar_url", "https://example.test/alice.png"},
                       {"public_key", "base64-x25519-public-key"}}
            .dump(),
        "application/json");
    ASSERT_TRUE(patchResult);
    ASSERT_EQ(patchResult->status, 200);

    const std::string login = nlohmann::json::parse(patchResult->body)["login"].get<std::string>();
    const httplib::Result getResult = client.Get("/users/" + login + "/profile", authHeader);
    ASSERT_TRUE(getResult);
    ASSERT_EQ(getResult->status, 200);
    nlohmann::json profile = nlohmann::json::parse(getResult->body);
    EXPECT_EQ(profile["display_name"].get<std::string>(), "Alice");
    EXPECT_EQ(profile["avatar_url"].get<std::string>(), "https://example.test/alice.png");
    EXPECT_EQ(profile["public_key"].get<std::string>(), "base64-x25519-public-key");

    // Частичное обновление: только display_name — avatar_url/public_key
    // должны сохраниться без изменений (ключ из issue #136 не должен
    // теряться из-за не связанного с ним редактирования профиля).
    const httplib::Result partialPatchResult =
        client.Patch("/users/me", authHeader, nlohmann::json{{"display_name", "Alice B."}}.dump(), "application/json");
    ASSERT_TRUE(partialPatchResult);
    ASSERT_EQ(partialPatchResult->status, 200);
    profile = nlohmann::json::parse(partialPatchResult->body);
    EXPECT_EQ(profile["display_name"].get<std::string>(), "Alice B.");
    EXPECT_EQ(profile["avatar_url"].get<std::string>(), "https://example.test/alice.png");
    EXPECT_EQ(profile["public_key"].get<std::string>(), "base64-x25519-public-key");
}

}  // namespace
}  // namespace user_service
