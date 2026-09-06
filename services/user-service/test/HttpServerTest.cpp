#include "HttpServer.h"

#include <gtest/gtest.h>
#include <httplib.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <nlohmann/json.hpp>
#include <optional>
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

TEST(HttpServerTest, ResolveOtpIdentifierRouteRejectsMissingFieldWith400) {
    UserRepository repository(connectionString());
    UserService userService(repository);
    const AuthServiceClient authServiceClient = testAuthServiceClient();
    const ScopedServer server(userService, authServiceClient);

    httplib::Client client(kTestHost, kTestPort);
    const httplib::Result result = client.Post("/users/resolve-otp-identifier", "{}", "application/json");

    ASSERT_TRUE(result);
    EXPECT_EQ(result->status, 400);
}

TEST(HttpServerTest, ResolveOtpIdentifierRouteReturnsNotFoundForUnknownIdentifier) {
    UserRepository repository(connectionString());
    UserService userService(repository);
    const AuthServiceClient authServiceClient = testAuthServiceClient();
    const ScopedServer server(userService, authServiceClient);

    httplib::Client client(kTestHost, kTestPort);
    const httplib::Result result = client.Post(
        "/users/resolve-otp-identifier",
        nlohmann::json{{"identifier", "no-such-identifier-ever-created@example.test"}}.dump(), "application/json");

    ASSERT_TRUE(result);
    ASSERT_EQ(result->status, 200);
    EXPECT_FALSE(nlohmann::json::parse(result->body)["found"].get<bool>());
}

TEST(HttpServerTest, ResolveOtpIdentifierRouteFindsUserByLoginOrEmailOnceEmailIsSet) {
    const std::string token = registerViaAuthServiceAndGetToken("http-server-otp-resolve-test");
    if (token.empty()) {
        GTEST_SKIP() << "auth-service (and the user-service it forwards to) not reachable — start the full stack.";
    }

    UserRepository repository(connectionString());
    UserService userService(repository);
    const AuthServiceClient authServiceClient = testAuthServiceClient();
    const ScopedServer server(userService, authServiceClient);

    httplib::Client client(kTestHost, kTestPort);
    httplib::Headers authHeader{{"Authorization", "Bearer " + token}};

    // Уникален на каждый запуск, как и uniqueLogin() — захардкоженный
    // литерал здесь столкнулся бы со строкой из предыдущего запуска на
    // частичном уникальном индексе (issue #156), как только этот
    // тестовый бинарник запустится второй раз на той же базе, превратив
    // PATCH ниже в никак не связанный с тестом 409.
    const std::string email = uniqueLogin("otp-resolve-test") + "@example.test";
    const httplib::Result patchResult =
        client.Patch("/users/me", authHeader, nlohmann::json{{"email", email}}.dump(), "application/json");
    ASSERT_TRUE(patchResult);
    ASSERT_EQ(patchResult->status, 200);
    const std::string login = nlohmann::json::parse(patchResult->body)["login"].get<std::string>();

    for (const std::string& identifier : {login, email}) {
        const httplib::Result resolveResult = client.Post(
            "/users/resolve-otp-identifier", nlohmann::json{{"identifier", identifier}}.dump(), "application/json");
        ASSERT_TRUE(resolveResult);
        ASSERT_EQ(resolveResult->status, 200);
        const nlohmann::json body = nlohmann::json::parse(resolveResult->body);
        EXPECT_TRUE(body["found"].get<bool>());
        EXPECT_EQ(body["login"].get<std::string>(), login);
        EXPECT_EQ(body["email"].get<std::string>(), email);
    }
}

TEST(HttpServerTest, ResolveOtpIdentifierRouteFindsUserByTelegramChatId) {
    const std::string token = registerViaAuthServiceAndGetToken("http-server-otp-telegram-resolve-test");
    if (token.empty()) {
        GTEST_SKIP() << "auth-service (and the user-service it forwards to) not reachable — start the full stack.";
    }

    UserRepository repository(connectionString());
    UserService userService(repository);
    const AuthServiceClient authServiceClient = testAuthServiceClient();
    const ScopedServer server(userService, authServiceClient);

    httplib::Client client(kTestHost, kTestPort);
    httplib::Headers authHeader{{"Authorization", "Bearer " + token}};

    const std::string chatId = uniqueLogin("otp-telegram-resolve-test");
    const httplib::Result patchResult = client.Patch("/users/me", authHeader,
                                                       nlohmann::json{{"telegram_chat_id", chatId}}.dump(),
                                                       "application/json");
    ASSERT_TRUE(patchResult);
    ASSERT_EQ(patchResult->status, 200);
    const std::string login = nlohmann::json::parse(patchResult->body)["login"].get<std::string>();

    const httplib::Result resolveResult = client.Post(
        "/users/resolve-otp-identifier", nlohmann::json{{"identifier", chatId}}.dump(), "application/json");
    ASSERT_TRUE(resolveResult);
    ASSERT_EQ(resolveResult->status, 200);
    const nlohmann::json body = nlohmann::json::parse(resolveResult->body);
    EXPECT_TRUE(body["found"].get<bool>());
    EXPECT_EQ(body["login"].get<std::string>(), login);
    EXPECT_EQ(body["telegram_chat_id"].get<std::string>(), chatId);
}

// Как registerViaAuthServiceAndGetToken(), но заявкам в друзья (issue
// #187) нужен ещё и сам login стороны, а не только токен — сервер
// определяет отправителя/адресата по логину, зашитому в токене, а не
// по тому, что передано в теле запроса.
struct FriendTestAccount {
    std::string login;
    std::string token;
};

std::optional<FriendTestAccount> registerFriendTestAccount(const std::string& loginPrefix) {
    const std::string authHost = envOrDefault("AUTH_SERVICE_HOST", "127.0.0.1");
    const int authPort = std::stoi(envOrDefault("AUTH_SERVICE_PORT", "8080"));
    httplib::Client client(authHost, authPort);
    const std::string login = uniqueLogin(loginPrefix);
    const nlohmann::json body{{"login", login}, {"password", "http-server-friends-test-password"}};
    const httplib::Result result = client.Post("/auth/register", body.dump(), "application/json");
    if (!result || result->status != 201) {
        return std::nullopt;
    }
    const nlohmann::json response = nlohmann::json::parse(result->body, nullptr, /*allow_exceptions=*/false);
    if (response.is_discarded() || !response.contains("token")) {
        return std::nullopt;
    }
    return FriendTestAccount{.login = login, .token = response["token"].get<std::string>()};
}

TEST(HttpServerTest, SendFriendRequestRouteRejectsMissingTokenWith401) {
    UserRepository repository(connectionString());
    UserService userService(repository);
    const AuthServiceClient authServiceClient = testAuthServiceClient();
    const ScopedServer server(userService, authServiceClient);

    httplib::Client client(kTestHost, kTestPort);
    const httplib::Result result =
        client.Post("/friends/requests", nlohmann::json{{"recipient_login", "anyone"}}.dump(), "application/json");

    ASSERT_TRUE(result);
    EXPECT_EQ(result->status, 401);
}

// Остальные сценарии (400/201/incoming-list/accept/friends-list)
// намеренно собраны в один тест на пару аккаунтов, а не разбиты по
// одному сценарию на тест, как везде выше в этом файле, — каждая
// registerFriendTestAccount() — это отдельный вызов POST
// /auth/register, а он делит один и тот же rate limit (10 запросов/60с
// на remote_addr, HttpServer.h::HttpServer()) с /auth/token и
// /auth/otp/* auth-service — при большом числе тестов, каждый из
// которых регистрирует собственную пару, набор тестов внутри одного
// 60-секундного окна легко превышает 10 и валит несвязанные соседние
// тесты 429-м (замечено на практике при первой версии этого файла).
TEST(HttpServerTest, SendFriendRequestRouteValidationAndAcceptRoundTrip) {
    const std::optional<FriendTestAccount> requester = registerFriendTestAccount("http-server-friend-a");
    const std::optional<FriendTestAccount> recipient = registerFriendTestAccount("http-server-friend-b");
    if (!requester.has_value() || !recipient.has_value()) {
        GTEST_SKIP() << "auth-service (and the user-service it forwards to) not reachable — start the full stack.";
    }

    UserRepository repository(connectionString());
    UserService userService(repository);
    const AuthServiceClient authServiceClient = testAuthServiceClient();
    const ScopedServer server(userService, authServiceClient);

    httplib::Client client(kTestHost, kTestPort);
    httplib::Headers requesterAuth{{"Authorization", "Bearer " + requester->token}};
    httplib::Headers recipientAuth{{"Authorization", "Bearer " + recipient->token}};

    const httplib::Result badBodyResult = client.Post("/friends/requests", requesterAuth, "{}", "application/json");
    ASSERT_TRUE(badBodyResult);
    EXPECT_EQ(badBodyResult->status, 400);

    const httplib::Result sendResult =
        client.Post("/friends/requests", requesterAuth,
                    nlohmann::json{{"recipient_login", recipient->login}}.dump(), "application/json");
    ASSERT_TRUE(sendResult);
    ASSERT_EQ(sendResult->status, 201);

    const nlohmann::json requests = nlohmann::json::parse(client.Get("/friends/requests", recipientAuth)->body);
    ASSERT_TRUE(requests.is_array());
    ASSERT_FALSE(requests.empty());
    EXPECT_TRUE(std::any_of(requests.begin(), requests.end(), [&](const nlohmann::json& item) {
        return item["requester_login"].get<std::string>() == requester->login;
    }));
    const std::int64_t requestId = requests[0]["id"].get<std::int64_t>();

    const httplib::Result acceptResult = client.Post("/friends/requests/" + std::to_string(requestId) + "/accept",
                                                       recipientAuth, "", "application/json");
    ASSERT_TRUE(acceptResult);
    EXPECT_EQ(acceptResult->status, 200);

    const nlohmann::json requesterFriends = nlohmann::json::parse(client.Get("/friends", requesterAuth)->body);
    const nlohmann::json recipientFriends = nlohmann::json::parse(client.Get("/friends", recipientAuth)->body);
    EXPECT_TRUE(std::find(requesterFriends.begin(), requesterFriends.end(), recipient->login) !=
                requesterFriends.end());
    EXPECT_TRUE(std::find(recipientFriends.begin(), recipientFriends.end(), requester->login) !=
                recipientFriends.end());
}

TEST(HttpServerTest, DeclineFriendRequestThenMutualRequestAndRemoveRoundTrip) {
    const std::optional<FriendTestAccount> requester = registerFriendTestAccount("http-server-friend-c");
    const std::optional<FriendTestAccount> recipient = registerFriendTestAccount("http-server-friend-d");
    if (!requester.has_value() || !recipient.has_value()) {
        GTEST_SKIP() << "auth-service (and the user-service it forwards to) not reachable — start the full stack.";
    }

    UserRepository repository(connectionString());
    UserService userService(repository);
    const AuthServiceClient authServiceClient = testAuthServiceClient();
    const ScopedServer server(userService, authServiceClient);

    httplib::Client client(kTestHost, kTestPort);
    httplib::Headers requesterAuth{{"Authorization", "Bearer " + requester->token}};
    httplib::Headers recipientAuth{{"Authorization", "Bearer " + recipient->token}};

    ASSERT_TRUE(client.Post("/friends/requests", requesterAuth,
                             nlohmann::json{{"recipient_login", recipient->login}}.dump(), "application/json"));
    const nlohmann::json requests = nlohmann::json::parse(client.Get("/friends/requests", recipientAuth)->body);
    ASSERT_FALSE(requests.empty());
    const std::int64_t requestId = requests[0]["id"].get<std::int64_t>();

    const httplib::Result declineResult = client.Post("/friends/requests/" + std::to_string(requestId) + "/decline",
                                                        recipientAuth, "", "application/json");
    ASSERT_TRUE(declineResult);
    EXPECT_EQ(declineResult->status, 200);
    EXPECT_TRUE(nlohmann::json::parse(client.Get("/friends", requesterAuth)->body).empty());

    // Отклонённая заявка не блокирует будущее сближение (issue #187) —
    // взаимная заявка сразу создаёт дружбу, без отдельного accept.
    ASSERT_TRUE(client.Post("/friends/requests", requesterAuth,
                             nlohmann::json{{"recipient_login", recipient->login}}.dump(), "application/json"));
    ASSERT_TRUE(client.Post("/friends/requests", recipientAuth,
                             nlohmann::json{{"recipient_login", requester->login}}.dump(), "application/json"));
    EXPECT_FALSE(nlohmann::json::parse(client.Get("/friends", requesterAuth)->body).empty());

    const httplib::Result removeResult = client.Delete("/friends/" + recipient->login, requesterAuth);
    ASSERT_TRUE(removeResult);
    EXPECT_EQ(removeResult->status, 200);
    EXPECT_TRUE(nlohmann::json::parse(client.Get("/friends", requesterAuth)->body).empty());
}

}  // namespace
}  // namespace user_service
