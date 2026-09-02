#include "AuthServiceClient.h"
#include "ChatRepository.h"
#include "ChatService.h"
#include "HttpServer.h"

#include <gtest/gtest.h>
#include <httplib.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <thread>
#include <vector>

// Тесты уровня маршрутов для HttpServer, обращающиеся к реальному
// экземпляру httplib::Server через loopback HTTP. Требуют работающий
// Postgres (собственный для chat-service, см. docker-compose.yml) И
// работающий auth-service (для выпуска настоящих токенов для заголовка
// Authorization) — те же зависимости, что и у WebSocketServerTest.cpp.
// Пропускают себя, а не падают, если что-то из этого недоступно.

namespace chat_service {
namespace {

std::string envOrDefault(const char* name, const std::string& defaultValue) {
    const char* value = std::getenv(name);
    return value != nullptr ? std::string(value) : defaultValue;
}

std::string dbConnectionString() {
    return envOrDefault("CHAT_SERVICE_DATABASE_URL",
                         "postgresql://chat_service:dev-only-password@localhost:5434/chat_service");
}

std::string uniqueSuffix() {
    return std::to_string(
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
            .count());
}

// Тот же хэлпер, что и в WebSocketServerTest.cpp: регистрирует нового
// пользователя в работающем auth-service и возвращает его токен.
std::optional<std::string> registerAndGetToken(const std::string& host, int port, const std::string& login) {
    httplib::Client client(host, port);
    const nlohmann::json body{{"login", login}, {"password", "http-server-test-password"}};
    const httplib::Result result = client.Post("/auth/register", body.dump(), "application/json");
    if (!result || result->status < 200 || result->status >= 300) {
        return std::nullopt;
    }
    const nlohmann::json response = nlohmann::json::parse(result->body, nullptr, /*allow_exceptions=*/false);
    if (response.is_discarded() || !response.contains("token")) {
        return std::nullopt;
    }
    return response["token"].get<std::string>();
}

constexpr const char* kTestHost = "127.0.0.1";
constexpr int kTestPort = 18082;

class ScopedServer {
public:
    ScopedServer(ChatService& chatService, const AuthServiceClient& authServiceClient)
        : server_(chatService, authServiceClient), thread_([this] { server_.listen(kTestHost, kTestPort); }) {
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

// Объединяет всё, что нужно тесту: репозиторий/auth-клиент, настоящий
// токен владельца — делает GTEST_SKIP вызывающего теста, если Postgres
// или auth-service недоступны. Намеренно НЕ хранит член ChatService:
// ChatService хранит ссылку на свой repository, а эта структура
// возвращается по значению через std::optional (перемещение), что
// оставило бы эту ссылку висячей — вместо этого каждый тест строит свой
// собственный локальный ChatService из fixture.repository.
struct TestFixture {
    std::string authHost;
    int authPort;
    ChatRepository repository;
    AuthServiceClient authServiceClient;
    std::string ownerLogin;
    std::string ownerToken;

    static std::optional<TestFixture> create(const std::string& loginPrefix) {
        const std::string authHost = envOrDefault("AUTH_SERVICE_HOST", "127.0.0.1");
        const int authPort = std::stoi(envOrDefault("AUTH_SERVICE_PORT", "8080"));
        const std::string suffix = uniqueSuffix();
        const std::string ownerLogin = loginPrefix + "-" + suffix;
        const std::optional<std::string> ownerToken = registerAndGetToken(authHost, authPort, ownerLogin);
        if (!ownerToken.has_value()) {
            return std::nullopt;
        }
        return TestFixture{authHost, authPort, ChatRepository(dbConnectionString()),
                            AuthServiceClient(authHost, authPort), ownerLogin, *ownerToken};
    }
};

std::string bearer(const std::string& token) {
    return "Bearer " + token;
}

TEST(HttpServerTest, ListCommunitiesRejectsMissingAuthorizationHeaderWith401) {
    const std::string authHost = envOrDefault("AUTH_SERVICE_HOST", "127.0.0.1");
    const int authPort = std::stoi(envOrDefault("AUTH_SERVICE_PORT", "8080"));
    if (!registerAndGetToken(authHost, authPort, "http-server-401-probe-" + uniqueSuffix()).has_value()) {
        GTEST_SKIP() << "auth-service not reachable — start it locally to run this test.";
    }

    ChatRepository repository(dbConnectionString());
    ChatService chatService(repository);
    const AuthServiceClient authServiceClient(authHost, authPort);
    const ScopedServer server(chatService, authServiceClient);

    httplib::Client client(kTestHost, kTestPort);
    const httplib::Result result = client.Get("/communities");

    ASSERT_TRUE(result);
    EXPECT_EQ(result->status, 401);
}

TEST(HttpServerTest, ListCommunitiesRejectsMalformedAuthorizationHeaderWith401) {
    const std::string authHost = envOrDefault("AUTH_SERVICE_HOST", "127.0.0.1");
    const int authPort = std::stoi(envOrDefault("AUTH_SERVICE_PORT", "8080"));
    if (!registerAndGetToken(authHost, authPort, "http-server-401-probe-" + uniqueSuffix()).has_value()) {
        GTEST_SKIP() << "auth-service not reachable — start it locally to run this test.";
    }

    ChatRepository repository(dbConnectionString());
    ChatService chatService(repository);
    const AuthServiceClient authServiceClient(authHost, authPort);
    const ScopedServer server(chatService, authServiceClient);

    httplib::Client client(kTestHost, kTestPort);
    httplib::Headers headers{{"Authorization", "not-a-bearer-header"}};
    const httplib::Result result = client.Get("/communities", headers);

    ASSERT_TRUE(result);
    EXPECT_EQ(result->status, 401);
}

TEST(HttpServerTest, ListCommunitiesRejectsInvalidTokenWith401) {
    const std::string authHost = envOrDefault("AUTH_SERVICE_HOST", "127.0.0.1");
    const int authPort = std::stoi(envOrDefault("AUTH_SERVICE_PORT", "8080"));
    if (!registerAndGetToken(authHost, authPort, "http-server-401-probe-" + uniqueSuffix()).has_value()) {
        GTEST_SKIP() << "auth-service not reachable — start it locally to run this test.";
    }

    ChatRepository repository(dbConnectionString());
    ChatService chatService(repository);
    const AuthServiceClient authServiceClient(authHost, authPort);
    const ScopedServer server(chatService, authServiceClient);

    httplib::Client client(kTestHost, kTestPort);
    httplib::Headers headers{{"Authorization", bearer("not-a-real-token")}};
    const httplib::Result result = client.Get("/communities", headers);

    ASSERT_TRUE(result);
    EXPECT_EQ(result->status, 401);
}

TEST(HttpServerTest, CreateCommunityRejectsMissingNameWith400) {
    auto fixtureOpt = TestFixture::create("http-server-create-community-400");
    if (!fixtureOpt.has_value()) {
        GTEST_SKIP() << "Postgres or auth-service not reachable — run `docker compose up` + start auth-service.";
    }
    auto& fixture = *fixtureOpt;
    ChatService chatService(fixture.repository);
    const ScopedServer server(chatService, fixture.authServiceClient);

    httplib::Client client(kTestHost, kTestPort);
    httplib::Headers headers{{"Authorization", bearer(fixture.ownerToken)}};
    const httplib::Result result = client.Post("/communities", headers, nlohmann::json::object().dump(), "application/json");

    ASSERT_TRUE(result);
    EXPECT_EQ(result->status, 400);
}

// Временная замена настоящему fuzzing'у, управляемому покрытием (issue
// #121): сильно вложенное тело JSON раньше переполняло стек всего
// процесса внутри рекурсивного спуска nlohmann::json::parse() (обрушивало
// эквивалентный вызов parse в WebSocketServer, обнаружено через
// WebSocketServerTest — см. JsonGuard.h). Это REST-эндпоинтная половина
// того же класса уязвимости; теперь оно должно отклоняться чистым 400, а не падением.
TEST(HttpServerTest, CreateCommunityRejectsDeeplyNestedBodyWith400) {
    auto fixtureOpt = TestFixture::create("http-server-create-community-nested-400");
    if (!fixtureOpt.has_value()) {
        GTEST_SKIP() << "Postgres or auth-service not reachable — run `docker compose up` + start auth-service.";
    }
    auto& fixture = *fixtureOpt;
    ChatService chatService(fixture.repository);
    const ScopedServer server(chatService, fixture.authServiceClient);

    httplib::Client client(kTestHost, kTestPort);
    httplib::Headers headers{{"Authorization", bearer(fixture.ownerToken)}};
    const std::string deeplyNestedBody(500, '[');
    const httplib::Result result = client.Post("/communities", headers, deeplyNestedBody, "application/json");

    ASSERT_TRUE(result);
    EXPECT_EQ(result->status, 400);
}

TEST(HttpServerTest, CreateCommunitySucceedsWith201) {
    auto fixtureOpt = TestFixture::create("http-server-create-community-201");
    if (!fixtureOpt.has_value()) {
        GTEST_SKIP() << "Postgres or auth-service not reachable — run `docker compose up` + start auth-service.";
    }
    auto& fixture = *fixtureOpt;
    ChatService chatService(fixture.repository);
    const ScopedServer server(chatService, fixture.authServiceClient);

    httplib::Client client(kTestHost, kTestPort);
    httplib::Headers headers{{"Authorization", bearer(fixture.ownerToken)}};
    const httplib::Result result =
        client.Post("/communities", headers, nlohmann::json{{"name", "http-test-community"}}.dump(), "application/json");

    ASSERT_TRUE(result);
    ASSERT_EQ(result->status, 201);
    const nlohmann::json body = nlohmann::json::parse(result->body);
    EXPECT_EQ(body["name"].get<std::string>(), "http-test-community");
    EXPECT_EQ(body["owner"].get<std::string>(), fixture.ownerLogin);
}

TEST(HttpServerTest, ListCommunitiesReturnsCreatedCommunity) {
    auto fixtureOpt = TestFixture::create("http-server-list-communities");
    if (!fixtureOpt.has_value()) {
        GTEST_SKIP() << "Postgres or auth-service not reachable — run `docker compose up` + start auth-service.";
    }
    auto& fixture = *fixtureOpt;
    ChatService chatService(fixture.repository);
    const std::string communityName = "http-test-listed-" + uniqueSuffix();
    const Community created = chatService.createCommunity(communityName, fixture.ownerLogin);

    const ScopedServer server(chatService, fixture.authServiceClient);
    httplib::Client client(kTestHost, kTestPort);
    httplib::Headers headers{{"Authorization", bearer(fixture.ownerToken)}};
    const httplib::Result result = client.Get("/communities", headers);

    ASSERT_TRUE(result);
    ASSERT_EQ(result->status, 200);
    const nlohmann::json body = nlohmann::json::parse(result->body);
    ASSERT_TRUE(body.is_array());
    const bool found = std::any_of(body.begin(), body.end(),
                                    [&](const nlohmann::json& entry) { return entry["id"].get<std::int64_t>() == created.id; });
    EXPECT_TRUE(found);
}

TEST(HttpServerTest, RenameCommunityRejectsMissingNameWith400) {
    auto fixtureOpt = TestFixture::create("http-server-rename-community-400");
    if (!fixtureOpt.has_value()) {
        GTEST_SKIP() << "Postgres or auth-service not reachable — run `docker compose up` + start auth-service.";
    }
    auto& fixture = *fixtureOpt;
    ChatService chatService(fixture.repository);
    const Community community = chatService.createCommunity("http-test-rc-400-" + uniqueSuffix(), fixture.ownerLogin);

    const ScopedServer server(chatService, fixture.authServiceClient);
    httplib::Client client(kTestHost, kTestPort);
    httplib::Headers headers{{"Authorization", bearer(fixture.ownerToken)}};
    const httplib::Result result =
        client.Patch("/communities/" + std::to_string(community.id), headers, nlohmann::json::object().dump(), "application/json");

    ASSERT_TRUE(result);
    EXPECT_EQ(result->status, 400);
}

TEST(HttpServerTest, RenameCommunitySucceedsForOwnerWith200) {
    auto fixtureOpt = TestFixture::create("http-server-rename-community-200");
    if (!fixtureOpt.has_value()) {
        GTEST_SKIP() << "Postgres or auth-service not reachable — run `docker compose up` + start auth-service.";
    }
    auto& fixture = *fixtureOpt;
    ChatService chatService(fixture.repository);
    const Community community = chatService.createCommunity("http-test-rc-200-" + uniqueSuffix(), fixture.ownerLogin);

    const ScopedServer server(chatService, fixture.authServiceClient);
    httplib::Client client(kTestHost, kTestPort);
    httplib::Headers headers{{"Authorization", bearer(fixture.ownerToken)}};
    const httplib::Result result = client.Patch("/communities/" + std::to_string(community.id), headers,
                                                  nlohmann::json{{"name", "renamed-via-http"}}.dump(), "application/json");

    ASSERT_TRUE(result);
    EXPECT_EQ(result->status, 200);
}

TEST(HttpServerTest, RenameCommunityRejectsNonOwnerWith403) {
    auto fixtureOpt = TestFixture::create("http-server-rename-community-403");
    if (!fixtureOpt.has_value()) {
        GTEST_SKIP() << "Postgres or auth-service not reachable — run `docker compose up` + start auth-service.";
    }
    auto& fixture = *fixtureOpt;
    ChatService chatService(fixture.repository);
    const Community community = chatService.createCommunity("http-test-rc-403-" + uniqueSuffix(), fixture.ownerLogin);
    const std::optional<std::string> intruderToken =
        registerAndGetToken(fixture.authHost, fixture.authPort, "http-server-intruder-" + uniqueSuffix());
    ASSERT_TRUE(intruderToken.has_value());

    const ScopedServer server(chatService, fixture.authServiceClient);
    httplib::Client client(kTestHost, kTestPort);
    httplib::Headers headers{{"Authorization", bearer(*intruderToken)}};
    const httplib::Result result = client.Patch("/communities/" + std::to_string(community.id), headers,
                                                  nlohmann::json{{"name", "hijacked"}}.dump(), "application/json");

    ASSERT_TRUE(result);
    EXPECT_EQ(result->status, 403);
}

TEST(HttpServerTest, RenameCommunityRejectsNonexistentIdWith404) {
    auto fixtureOpt = TestFixture::create("http-server-rename-community-404");
    if (!fixtureOpt.has_value()) {
        GTEST_SKIP() << "Postgres or auth-service not reachable — run `docker compose up` + start auth-service.";
    }
    auto& fixture = *fixtureOpt;
    ChatService chatService(fixture.repository);
    const ScopedServer server(chatService, fixture.authServiceClient);

    httplib::Client client(kTestHost, kTestPort);
    httplib::Headers headers{{"Authorization", bearer(fixture.ownerToken)}};
    const httplib::Result result =
        client.Patch("/communities/999999999", headers, nlohmann::json{{"name", "nowhere"}}.dump(), "application/json");

    ASSERT_TRUE(result);
    EXPECT_EQ(result->status, 404);
}

TEST(HttpServerTest, DeleteCommunitySucceedsForOwnerWith200) {
    auto fixtureOpt = TestFixture::create("http-server-delete-community-200");
    if (!fixtureOpt.has_value()) {
        GTEST_SKIP() << "Postgres or auth-service not reachable — run `docker compose up` + start auth-service.";
    }
    auto& fixture = *fixtureOpt;
    ChatService chatService(fixture.repository);
    const Community community = chatService.createCommunity("http-test-dc-200-" + uniqueSuffix(), fixture.ownerLogin);

    const ScopedServer server(chatService, fixture.authServiceClient);
    httplib::Client client(kTestHost, kTestPort);
    httplib::Headers headers{{"Authorization", bearer(fixture.ownerToken)}};
    const httplib::Result result = client.Delete("/communities/" + std::to_string(community.id), headers);

    ASSERT_TRUE(result);
    EXPECT_EQ(result->status, 200);
}

TEST(HttpServerTest, DeleteCommunityRejectsNonOwnerWith403) {
    auto fixtureOpt = TestFixture::create("http-server-delete-community-403");
    if (!fixtureOpt.has_value()) {
        GTEST_SKIP() << "Postgres or auth-service not reachable — run `docker compose up` + start auth-service.";
    }
    auto& fixture = *fixtureOpt;
    ChatService chatService(fixture.repository);
    const Community community = chatService.createCommunity("http-test-dc-403-" + uniqueSuffix(), fixture.ownerLogin);
    const std::optional<std::string> intruderToken =
        registerAndGetToken(fixture.authHost, fixture.authPort, "http-server-dc-intruder-" + uniqueSuffix());
    ASSERT_TRUE(intruderToken.has_value());

    const ScopedServer server(chatService, fixture.authServiceClient);
    httplib::Client client(kTestHost, kTestPort);
    httplib::Headers headers{{"Authorization", bearer(*intruderToken)}};
    const httplib::Result result = client.Delete("/communities/" + std::to_string(community.id), headers);

    ASSERT_TRUE(result);
    EXPECT_EQ(result->status, 403);
}

TEST(HttpServerTest, DeleteCommunityRejectsNonexistentIdWith404) {
    auto fixtureOpt = TestFixture::create("http-server-delete-community-404");
    if (!fixtureOpt.has_value()) {
        GTEST_SKIP() << "Postgres or auth-service not reachable — run `docker compose up` + start auth-service.";
    }
    auto& fixture = *fixtureOpt;
    ChatService chatService(fixture.repository);
    const ScopedServer server(chatService, fixture.authServiceClient);

    httplib::Client client(kTestHost, kTestPort);
    httplib::Headers headers{{"Authorization", bearer(fixture.ownerToken)}};
    const httplib::Result result = client.Delete("/communities/999999999", headers);

    ASSERT_TRUE(result);
    EXPECT_EQ(result->status, 404);
}

TEST(HttpServerTest, JoinCommunitySucceedsWith200) {
    auto fixtureOpt = TestFixture::create("http-server-join-community-200");
    if (!fixtureOpt.has_value()) {
        GTEST_SKIP() << "Postgres or auth-service not reachable — run `docker compose up` + start auth-service.";
    }
    auto& fixture = *fixtureOpt;
    ChatService chatService(fixture.repository);
    const Community community = chatService.createCommunity("http-test-join-200-" + uniqueSuffix(), fixture.ownerLogin);
    const std::optional<std::string> joinerToken =
        registerAndGetToken(fixture.authHost, fixture.authPort, "http-server-joiner-" + uniqueSuffix());
    ASSERT_TRUE(joinerToken.has_value());

    const ScopedServer server(chatService, fixture.authServiceClient);
    httplib::Client client(kTestHost, kTestPort);
    httplib::Headers headers{{"Authorization", bearer(*joinerToken)}};
    const httplib::Result result = client.Post("/communities/" + std::to_string(community.id) + "/join", headers, "", "application/json");

    ASSERT_TRUE(result);
    EXPECT_EQ(result->status, 200);
}

TEST(HttpServerTest, JoinCommunityRejectsNonexistentIdWith404) {
    auto fixtureOpt = TestFixture::create("http-server-join-community-404");
    if (!fixtureOpt.has_value()) {
        GTEST_SKIP() << "Postgres or auth-service not reachable — run `docker compose up` + start auth-service.";
    }
    auto& fixture = *fixtureOpt;
    ChatService chatService(fixture.repository);
    const ScopedServer server(chatService, fixture.authServiceClient);

    httplib::Client client(kTestHost, kTestPort);
    httplib::Headers headers{{"Authorization", bearer(fixture.ownerToken)}};
    const httplib::Result result = client.Post("/communities/999999999/join", headers, "", "application/json");

    ASSERT_TRUE(result);
    EXPECT_EQ(result->status, 404);
}

TEST(HttpServerTest, CreateChannelRejectsMissingNameWith400) {
    auto fixtureOpt = TestFixture::create("http-server-create-channel-400");
    if (!fixtureOpt.has_value()) {
        GTEST_SKIP() << "Postgres or auth-service not reachable — run `docker compose up` + start auth-service.";
    }
    auto& fixture = *fixtureOpt;
    ChatService chatService(fixture.repository);
    const Community community = chatService.createCommunity("http-test-cc-400-" + uniqueSuffix(), fixture.ownerLogin);

    const ScopedServer server(chatService, fixture.authServiceClient);
    httplib::Client client(kTestHost, kTestPort);
    httplib::Headers headers{{"Authorization", bearer(fixture.ownerToken)}};
    const httplib::Result result = client.Post("/communities/" + std::to_string(community.id) + "/channels", headers,
                                                 nlohmann::json::object().dump(), "application/json");

    ASSERT_TRUE(result);
    EXPECT_EQ(result->status, 400);
}

TEST(HttpServerTest, CreateChannelSucceedsWith201) {
    auto fixtureOpt = TestFixture::create("http-server-create-channel-201");
    if (!fixtureOpt.has_value()) {
        GTEST_SKIP() << "Postgres or auth-service not reachable — run `docker compose up` + start auth-service.";
    }
    auto& fixture = *fixtureOpt;
    ChatService chatService(fixture.repository);
    const Community community = chatService.createCommunity("http-test-cc-201-" + uniqueSuffix(), fixture.ownerLogin);

    const ScopedServer server(chatService, fixture.authServiceClient);
    httplib::Client client(kTestHost, kTestPort);
    httplib::Headers headers{{"Authorization", bearer(fixture.ownerToken)}};
    const httplib::Result result = client.Post("/communities/" + std::to_string(community.id) + "/channels", headers,
                                                 nlohmann::json{{"name", "general"}}.dump(), "application/json");

    ASSERT_TRUE(result);
    EXPECT_EQ(result->status, 201);
}

TEST(HttpServerTest, CreateChannelRejectsNonexistentCommunityWith404) {
    auto fixtureOpt = TestFixture::create("http-server-create-channel-404");
    if (!fixtureOpt.has_value()) {
        GTEST_SKIP() << "Postgres or auth-service not reachable — run `docker compose up` + start auth-service.";
    }
    auto& fixture = *fixtureOpt;
    ChatService chatService(fixture.repository);
    const ScopedServer server(chatService, fixture.authServiceClient);

    httplib::Client client(kTestHost, kTestPort);
    httplib::Headers headers{{"Authorization", bearer(fixture.ownerToken)}};
    const httplib::Result result =
        client.Post("/communities/999999999/channels", headers, nlohmann::json{{"name", "nowhere"}}.dump(), "application/json");

    ASSERT_TRUE(result);
    EXPECT_EQ(result->status, 404);
}

TEST(HttpServerTest, ListChannelsReturnsCreatedChannel) {
    auto fixtureOpt = TestFixture::create("http-server-list-channels");
    if (!fixtureOpt.has_value()) {
        GTEST_SKIP() << "Postgres or auth-service not reachable — run `docker compose up` + start auth-service.";
    }
    auto& fixture = *fixtureOpt;
    ChatService chatService(fixture.repository);
    const Community community = chatService.createCommunity("http-test-lc-" + uniqueSuffix(), fixture.ownerLogin);
    const std::optional<std::int64_t> channelId = chatService.createChannel(community.id, "general", fixture.ownerLogin);
    ASSERT_TRUE(channelId.has_value());

    const ScopedServer server(chatService, fixture.authServiceClient);
    httplib::Client client(kTestHost, kTestPort);
    httplib::Headers headers{{"Authorization", bearer(fixture.ownerToken)}};
    const httplib::Result result = client.Get("/communities/" + std::to_string(community.id) + "/channels", headers);

    ASSERT_TRUE(result);
    ASSERT_EQ(result->status, 200);
    const nlohmann::json body = nlohmann::json::parse(result->body);
    ASSERT_EQ(body.size(), 1U);
    EXPECT_EQ(body[0]["name"].get<std::string>(), "general");
}

TEST(HttpServerTest, RenameChannelRejectsDuplicateNameWith409) {
    auto fixtureOpt = TestFixture::create("http-server-rename-channel-409");
    if (!fixtureOpt.has_value()) {
        GTEST_SKIP() << "Postgres or auth-service not reachable — run `docker compose up` + start auth-service.";
    }
    auto& fixture = *fixtureOpt;
    ChatService chatService(fixture.repository);
    const Community community = chatService.createCommunity("http-test-rn409-" + uniqueSuffix(), fixture.ownerLogin);
    const std::optional<std::int64_t> firstChannelId =
        chatService.createChannel(community.id, "general", fixture.ownerLogin);
    const std::optional<std::int64_t> secondChannelId =
        chatService.createChannel(community.id, "random", fixture.ownerLogin);
    ASSERT_TRUE(firstChannelId.has_value());
    ASSERT_TRUE(secondChannelId.has_value());

    const ScopedServer server(chatService, fixture.authServiceClient);
    httplib::Client client(kTestHost, kTestPort);
    httplib::Headers headers{{"Authorization", bearer(fixture.ownerToken)}};
    const httplib::Result result = client.Patch("/channels/" + std::to_string(*secondChannelId), headers,
                                                  nlohmann::json{{"name", "general"}}.dump(), "application/json");

    ASSERT_TRUE(result);
    EXPECT_EQ(result->status, 409);
}

TEST(HttpServerTest, RenameChannelSucceedsForOwnerWith200) {
    auto fixtureOpt = TestFixture::create("http-server-rename-channel-200");
    if (!fixtureOpt.has_value()) {
        GTEST_SKIP() << "Postgres or auth-service not reachable — run `docker compose up` + start auth-service.";
    }
    auto& fixture = *fixtureOpt;
    ChatService chatService(fixture.repository);
    const Community community = chatService.createCommunity("http-test-rn200-" + uniqueSuffix(), fixture.ownerLogin);
    const std::optional<std::int64_t> channelId = chatService.createChannel(community.id, "general", fixture.ownerLogin);
    ASSERT_TRUE(channelId.has_value());

    const ScopedServer server(chatService, fixture.authServiceClient);
    httplib::Client client(kTestHost, kTestPort);
    httplib::Headers headers{{"Authorization", bearer(fixture.ownerToken)}};
    const httplib::Result result = client.Patch("/channels/" + std::to_string(*channelId), headers,
                                                  nlohmann::json{{"name", "renamed"}}.dump(), "application/json");

    ASSERT_TRUE(result);
    EXPECT_EQ(result->status, 200);
}

TEST(HttpServerTest, DeleteChannelSucceedsForOwnerWith200) {
    auto fixtureOpt = TestFixture::create("http-server-delete-channel-200");
    if (!fixtureOpt.has_value()) {
        GTEST_SKIP() << "Postgres or auth-service not reachable — run `docker compose up` + start auth-service.";
    }
    auto& fixture = *fixtureOpt;
    ChatService chatService(fixture.repository);
    const Community community = chatService.createCommunity("http-test-dch200-" + uniqueSuffix(), fixture.ownerLogin);
    const std::optional<std::int64_t> channelId = chatService.createChannel(community.id, "general", fixture.ownerLogin);
    ASSERT_TRUE(channelId.has_value());

    const ScopedServer server(chatService, fixture.authServiceClient);
    httplib::Client client(kTestHost, kTestPort);
    httplib::Headers headers{{"Authorization", bearer(fixture.ownerToken)}};
    const httplib::Result result = client.Delete("/channels/" + std::to_string(*channelId), headers);

    ASSERT_TRUE(result);
    EXPECT_EQ(result->status, 200);
}

TEST(HttpServerTest, DeleteChannelRejectsNonOwnerWith403) {
    auto fixtureOpt = TestFixture::create("http-server-delete-channel-403");
    if (!fixtureOpt.has_value()) {
        GTEST_SKIP() << "Postgres or auth-service not reachable — run `docker compose up` + start auth-service.";
    }
    auto& fixture = *fixtureOpt;
    ChatService chatService(fixture.repository);
    const Community community = chatService.createCommunity("http-test-dch403-" + uniqueSuffix(), fixture.ownerLogin);
    const std::optional<std::int64_t> channelId = chatService.createChannel(community.id, "general", fixture.ownerLogin);
    ASSERT_TRUE(channelId.has_value());
    const std::optional<std::string> intruderToken =
        registerAndGetToken(fixture.authHost, fixture.authPort, "http-server-dch-intruder-" + uniqueSuffix());
    ASSERT_TRUE(intruderToken.has_value());

    const ScopedServer server(chatService, fixture.authServiceClient);
    httplib::Client client(kTestHost, kTestPort);
    httplib::Headers headers{{"Authorization", bearer(*intruderToken)}};
    const httplib::Result result = client.Delete("/channels/" + std::to_string(*channelId), headers);

    ASSERT_TRUE(result);
    EXPECT_EQ(result->status, 403);
}

TEST(HttpServerTest, ListMessagesReturnsPostedMessageWithDefaultLimit) {
    auto fixtureOpt = TestFixture::create("http-server-list-messages");
    if (!fixtureOpt.has_value()) {
        GTEST_SKIP() << "Postgres or auth-service not reachable — run `docker compose up` + start auth-service.";
    }
    auto& fixture = *fixtureOpt;
    ChatService chatService(fixture.repository);
    const Community community = chatService.createCommunity("http-test-lm-" + uniqueSuffix(), fixture.ownerLogin);
    const std::optional<std::int64_t> channelId = chatService.createChannel(community.id, "general", fixture.ownerLogin);
    ASSERT_TRUE(channelId.has_value());
    ASSERT_TRUE(chatService.postMessage(*channelId, fixture.ownerLogin, "hello via http").has_value());

    const ScopedServer server(chatService, fixture.authServiceClient);
    httplib::Client client(kTestHost, kTestPort);
    httplib::Headers headers{{"Authorization", bearer(fixture.ownerToken)}};
    const httplib::Result result = client.Get("/channels/" + std::to_string(*channelId) + "/messages", headers);

    ASSERT_TRUE(result);
    ASSERT_EQ(result->status, 200);
    const nlohmann::json body = nlohmann::json::parse(result->body);
    ASSERT_EQ(body.size(), 1U);
    EXPECT_EQ(body[0]["body"].get<std::string>(), "hello via http");
}

TEST(HttpServerTest, ListMessagesRespectsLimitQueryParam) {
    auto fixtureOpt = TestFixture::create("http-server-list-messages-limit");
    if (!fixtureOpt.has_value()) {
        GTEST_SKIP() << "Postgres or auth-service not reachable — run `docker compose up` + start auth-service.";
    }
    auto& fixture = *fixtureOpt;
    ChatService chatService(fixture.repository);
    const Community community = chatService.createCommunity("http-test-lml-" + uniqueSuffix(), fixture.ownerLogin);
    const std::optional<std::int64_t> channelId = chatService.createChannel(community.id, "general", fixture.ownerLogin);
    ASSERT_TRUE(channelId.has_value());
    ASSERT_TRUE(chatService.postMessage(*channelId, fixture.ownerLogin, "first").has_value());
    ASSERT_TRUE(chatService.postMessage(*channelId, fixture.ownerLogin, "second").has_value());

    const ScopedServer server(chatService, fixture.authServiceClient);
    httplib::Client client(kTestHost, kTestPort);
    httplib::Headers headers{{"Authorization", bearer(fixture.ownerToken)}};
    const httplib::Result result = client.Get("/channels/" + std::to_string(*channelId) + "/messages?limit=1", headers);

    ASSERT_TRUE(result);
    ASSERT_EQ(result->status, 200);
    const nlohmann::json body = nlohmann::json::parse(result->body);
    EXPECT_EQ(body.size(), 1U);
}

TEST(HttpServerTest, PromoteModeratorRejectsNonOwnerWith403) {
    auto fixtureOpt = TestFixture::create("http-server-promote-403");
    if (!fixtureOpt.has_value()) {
        GTEST_SKIP() << "Postgres or auth-service not reachable — run `docker compose up` + start auth-service.";
    }
    auto& fixture = *fixtureOpt;
    ChatService chatService(fixture.repository);
    const Community community = chatService.createCommunity("http-test-mod-403-" + uniqueSuffix(), fixture.ownerLogin);
    const std::optional<std::string> intruderToken =
        registerAndGetToken(fixture.authHost, fixture.authPort, "http-server-mod-intruder-" + uniqueSuffix());
    ASSERT_TRUE(intruderToken.has_value());

    const ScopedServer server(chatService, fixture.authServiceClient);
    httplib::Client client(kTestHost, kTestPort);
    httplib::Headers headers{{"Authorization", bearer(*intruderToken)}};
    const httplib::Result result =
        client.Post("/communities/" + std::to_string(community.id) + "/moderators", headers,
                    nlohmann::json{{"login", "anyone"}}.dump(), "application/json");

    ASSERT_TRUE(result);
    EXPECT_EQ(result->status, 403);
}

TEST(HttpServerTest, PromoteThenListThenDemoteModeratorRoundTrip) {
    auto fixtureOpt = TestFixture::create("http-server-promote-200");
    if (!fixtureOpt.has_value()) {
        GTEST_SKIP() << "Postgres or auth-service not reachable — run `docker compose up` + start auth-service.";
    }
    auto& fixture = *fixtureOpt;
    ChatService chatService(fixture.repository);
    const Community community = chatService.createCommunity("http-test-mod-200-" + uniqueSuffix(), fixture.ownerLogin);
    const std::string target = "http-server-mod-target-" + uniqueSuffix();

    const ScopedServer server(chatService, fixture.authServiceClient);
    httplib::Client client(kTestHost, kTestPort);
    httplib::Headers headers{{"Authorization", bearer(fixture.ownerToken)}};

    const httplib::Result promoteResult =
        client.Post("/communities/" + std::to_string(community.id) + "/moderators", headers,
                    nlohmann::json{{"login", target}}.dump(), "application/json");
    ASSERT_TRUE(promoteResult);
    EXPECT_EQ(promoteResult->status, 200);

    const httplib::Result listResult = client.Get("/communities/" + std::to_string(community.id) + "/moderators", headers);
    ASSERT_TRUE(listResult);
    ASSERT_EQ(listResult->status, 200);
    const nlohmann::json moderators = nlohmann::json::parse(listResult->body);
    ASSERT_EQ(moderators.size(), 1U);
    EXPECT_EQ(moderators[0].get<std::string>(), target);

    const httplib::Result demoteResult =
        client.Delete("/communities/" + std::to_string(community.id) + "/moderators/" + target, headers);
    ASSERT_TRUE(demoteResult);
    EXPECT_EQ(demoteResult->status, 200);

    const httplib::Result listAfterDemoteResult =
        client.Get("/communities/" + std::to_string(community.id) + "/moderators", headers);
    ASSERT_TRUE(listAfterDemoteResult);
    EXPECT_EQ(nlohmann::json::parse(listAfterDemoteResult->body).size(), 0U);
}

TEST(HttpServerTest, UploadAttachmentRejectsMissingFieldsWith400) {
    auto fixtureOpt = TestFixture::create("http-server-upload-400");
    if (!fixtureOpt.has_value()) {
        GTEST_SKIP() << "Postgres or auth-service not reachable — run `docker compose up` + start auth-service.";
    }
    auto& fixture = *fixtureOpt;
    ChatService chatService(fixture.repository);
    const Community community = chatService.createCommunity("http-test-upload-400-" + uniqueSuffix(), fixture.ownerLogin);
    const std::optional<std::int64_t> channelId =
        chatService.createChannel(community.id, "general", fixture.ownerLogin);
    ASSERT_TRUE(channelId.has_value());

    const ScopedServer server(chatService, fixture.authServiceClient);
    httplib::Client client(kTestHost, kTestPort);
    httplib::Headers headers{{"Authorization", bearer(fixture.ownerToken)}};
    const httplib::Result result = client.Post("/channels/" + std::to_string(*channelId) + "/attachments", headers,
                                                 nlohmann::json{{"filename", "test.txt"}}.dump(), "application/json");

    ASSERT_TRUE(result);
    EXPECT_EQ(result->status, 400);
}

TEST(HttpServerTest, UploadAttachmentRejectsInvalidBase64With400) {
    auto fixtureOpt = TestFixture::create("http-server-upload-badb64");
    if (!fixtureOpt.has_value()) {
        GTEST_SKIP() << "Postgres or auth-service not reachable — run `docker compose up` + start auth-service.";
    }
    auto& fixture = *fixtureOpt;
    ChatService chatService(fixture.repository);
    const Community community =
        chatService.createCommunity("http-test-upload-badb64-" + uniqueSuffix(), fixture.ownerLogin);
    const std::optional<std::int64_t> channelId =
        chatService.createChannel(community.id, "general", fixture.ownerLogin);
    ASSERT_TRUE(channelId.has_value());

    const ScopedServer server(chatService, fixture.authServiceClient);
    httplib::Client client(kTestHost, kTestPort);
    httplib::Headers headers{{"Authorization", bearer(fixture.ownerToken)}};
    const httplib::Result result = client.Post(
        "/channels/" + std::to_string(*channelId) + "/attachments", headers,
        nlohmann::json{{"filename", "test.txt"}, {"content_type", "text/plain"}, {"data_base64", "not!valid$$$"}}
            .dump(),
        "application/json");

    ASSERT_TRUE(result);
    EXPECT_EQ(result->status, 400);
}

TEST(HttpServerTest, UploadAttachmentRejectsOversizedPayloadWith400) {
    auto fixtureOpt = TestFixture::create("http-server-upload-oversized");
    if (!fixtureOpt.has_value()) {
        GTEST_SKIP() << "Postgres or auth-service not reachable — run `docker compose up` + start auth-service.";
    }
    auto& fixture = *fixtureOpt;
    ChatService chatService(fixture.repository);
    const Community community =
        chatService.createCommunity("http-test-upload-oversized-" + uniqueSuffix(), fixture.ownerLogin);
    const std::optional<std::int64_t> channelId =
        chatService.createChannel(community.id, "general", fixture.ownerLogin);
    ASSERT_TRUE(channelId.has_value());

    // 7 000 000 символов 'A' декодируются без остатка (делится на 4) в
    // 5 250 000 нулевых байт — сверх лимита в 5 МБ (5 242 880 байт).
    const std::string oversizedBase64(7000000, 'A');

    const ScopedServer server(chatService, fixture.authServiceClient);
    httplib::Client client(kTestHost, kTestPort);
    httplib::Headers headers{{"Authorization", bearer(fixture.ownerToken)}};
    const httplib::Result result = client.Post(
        "/channels/" + std::to_string(*channelId) + "/attachments", headers,
        nlohmann::json{{"filename", "big.bin"}, {"content_type", "application/octet-stream"},
                       {"data_base64", oversizedBase64}}
            .dump(),
        "application/json");

    ASSERT_TRUE(result);
    EXPECT_EQ(result->status, 400);
}

TEST(HttpServerTest, UploadThenDownloadAttachmentRoundTrip) {
    auto fixtureOpt = TestFixture::create("http-server-upload-200");
    if (!fixtureOpt.has_value()) {
        GTEST_SKIP() << "Postgres or auth-service not reachable — run `docker compose up` + start auth-service.";
    }
    auto& fixture = *fixtureOpt;
    ChatService chatService(fixture.repository);
    const Community community = chatService.createCommunity("http-test-upload-200-" + uniqueSuffix(), fixture.ownerLogin);
    const std::optional<std::int64_t> channelId =
        chatService.createChannel(community.id, "general", fixture.ownerLogin);
    ASSERT_TRUE(channelId.has_value());

    const ScopedServer server(chatService, fixture.authServiceClient);
    httplib::Client client(kTestHost, kTestPort);
    httplib::Headers headers{{"Authorization", bearer(fixture.ownerToken)}};

    // "Hello, attachment!" в base64.
    const httplib::Result uploadResult = client.Post(
        "/channels/" + std::to_string(*channelId) + "/attachments", headers,
        nlohmann::json{{"filename", "greeting.txt"},
                       {"content_type", "text/plain"},
                       {"data_base64", "SGVsbG8sIGF0dGFjaG1lbnQh"}}
            .dump(),
        "application/json");
    ASSERT_TRUE(uploadResult);
    ASSERT_EQ(uploadResult->status, 201);
    const nlohmann::json uploadBody = nlohmann::json::parse(uploadResult->body);
    EXPECT_EQ(uploadBody["filename"].get<std::string>(), "greeting.txt");
    EXPECT_EQ(uploadBody["content_type"].get<std::string>(), "text/plain");
    EXPECT_EQ(uploadBody["size_bytes"].get<std::int64_t>(), 18);
    const auto attachmentId = uploadBody["id"].get<std::int64_t>();
    ASSERT_GT(attachmentId, 0);

    const httplib::Result downloadResult = client.Get("/attachments/" + std::to_string(attachmentId), headers);
    ASSERT_TRUE(downloadResult);
    ASSERT_EQ(downloadResult->status, 200);
    EXPECT_EQ(downloadResult->body, "Hello, attachment!");
    EXPECT_EQ(downloadResult->get_header_value("Content-Type"), "text/plain");
    EXPECT_NE(downloadResult->get_header_value("Content-Disposition").find("greeting.txt"), std::string::npos);
}

TEST(HttpServerTest, DownloadAttachmentRejectsNonexistentIdWith404) {
    auto fixtureOpt = TestFixture::create("http-server-download-404");
    if (!fixtureOpt.has_value()) {
        GTEST_SKIP() << "Postgres or auth-service not reachable — run `docker compose up` + start auth-service.";
    }
    auto& fixture = *fixtureOpt;
    ChatService chatService(fixture.repository);

    const ScopedServer server(chatService, fixture.authServiceClient);
    httplib::Client client(kTestHost, kTestPort);
    httplib::Headers headers{{"Authorization", bearer(fixture.ownerToken)}};
    const httplib::Result result = client.Get("/attachments/999999999", headers);

    ASSERT_TRUE(result);
    EXPECT_EQ(result->status, 404);
}

TEST(HttpServerTest, SearchMessagesReturnsOnlyMatchingBodiesNewestFirst) {
    auto fixtureOpt = TestFixture::create("http-server-search-messages");
    if (!fixtureOpt.has_value()) {
        GTEST_SKIP() << "Postgres or auth-service not reachable — run `docker compose up` + start auth-service.";
    }
    auto& fixture = *fixtureOpt;
    ChatService chatService(fixture.repository);
    const Community community = chatService.createCommunity("http-test-search-" + uniqueSuffix(), fixture.ownerLogin);
    const std::optional<std::int64_t> channelId = chatService.createChannel(community.id, "general", fixture.ownerLogin);
    ASSERT_TRUE(channelId.has_value());
    ASSERT_TRUE(chatService.postMessage(*channelId, fixture.ownerLogin, "the quick brown fox").has_value());
    ASSERT_TRUE(chatService.postMessage(*channelId, fixture.ownerLogin, "nothing relevant here").has_value());
    ASSERT_TRUE(chatService.postMessage(*channelId, fixture.ownerLogin, "another Quick message").has_value());

    const ScopedServer server(chatService, fixture.authServiceClient);
    httplib::Client client(kTestHost, kTestPort);
    httplib::Headers headers{{"Authorization", bearer(fixture.ownerToken)}};
    const httplib::Result result =
        client.Get("/channels/" + std::to_string(*channelId) + "/messages/search?q=quick", headers);

    ASSERT_TRUE(result);
    ASSERT_EQ(result->status, 200);
    const nlohmann::json body = nlohmann::json::parse(result->body);
    ASSERT_EQ(body.size(), 2U);
    // Регистронезависимо, сначала самое новое совпадение.
    EXPECT_EQ(body[0]["body"].get<std::string>(), "another Quick message");
    EXPECT_EQ(body[1]["body"].get<std::string>(), "the quick brown fox");
}

TEST(HttpServerTest, SearchMessagesRejectsMissingQueryParamWith400) {
    auto fixtureOpt = TestFixture::create("http-server-search-missing-q");
    if (!fixtureOpt.has_value()) {
        GTEST_SKIP() << "Postgres or auth-service not reachable — run `docker compose up` + start auth-service.";
    }
    auto& fixture = *fixtureOpt;
    ChatService chatService(fixture.repository);
    const Community community = chatService.createCommunity("http-test-search-400-" + uniqueSuffix(), fixture.ownerLogin);
    const std::optional<std::int64_t> channelId = chatService.createChannel(community.id, "general", fixture.ownerLogin);
    ASSERT_TRUE(channelId.has_value());

    const ScopedServer server(chatService, fixture.authServiceClient);
    httplib::Client client(kTestHost, kTestPort);
    httplib::Headers headers{{"Authorization", bearer(fixture.ownerToken)}};
    const httplib::Result result = client.Get("/channels/" + std::to_string(*channelId) + "/messages/search", headers);

    ASSERT_TRUE(result);
    EXPECT_EQ(result->status, 400);
}

TEST(HttpServerTest, CreateChannelWithIsEncryptedTrueSetsTheFlagInResponseAndListing) {
    auto fixtureOpt = TestFixture::create("http-server-create-encrypted-channel");
    if (!fixtureOpt.has_value()) {
        GTEST_SKIP() << "Postgres or auth-service not reachable — run `docker compose up` + start auth-service.";
    }
    auto& fixture = *fixtureOpt;
    ChatService chatService(fixture.repository);
    const Community community =
        chatService.createCommunity("http-test-enc-channel-" + uniqueSuffix(), fixture.ownerLogin);

    const ScopedServer server(chatService, fixture.authServiceClient);
    httplib::Client client(kTestHost, kTestPort);
    httplib::Headers headers{{"Authorization", bearer(fixture.ownerToken)}};
    const httplib::Result createResult =
        client.Post("/communities/" + std::to_string(community.id) + "/channels", headers,
                    nlohmann::json{{"name", "secret"}, {"is_encrypted", true}}.dump(), "application/json");
    ASSERT_TRUE(createResult);
    ASSERT_EQ(createResult->status, 201);
    EXPECT_TRUE(nlohmann::json::parse(createResult->body)["is_encrypted"].get<bool>());

    const httplib::Result listResult =
        client.Get("/communities/" + std::to_string(community.id) + "/channels", headers);
    ASSERT_TRUE(listResult);
    const nlohmann::json channels = nlohmann::json::parse(listResult->body);
    ASSERT_EQ(channels.size(), 1U);
    EXPECT_TRUE(channels[0]["is_encrypted"].get<bool>());
}

TEST(HttpServerTest, CreateChannelWithoutIsEncryptedDefaultsToFalse) {
    auto fixtureOpt = TestFixture::create("http-server-create-plain-channel");
    if (!fixtureOpt.has_value()) {
        GTEST_SKIP() << "Postgres or auth-service not reachable — run `docker compose up` + start auth-service.";
    }
    auto& fixture = *fixtureOpt;
    ChatService chatService(fixture.repository);
    const Community community =
        chatService.createCommunity("http-test-plain-channel-" + uniqueSuffix(), fixture.ownerLogin);

    const ScopedServer server(chatService, fixture.authServiceClient);
    httplib::Client client(kTestHost, kTestPort);
    httplib::Headers headers{{"Authorization", bearer(fixture.ownerToken)}};
    const httplib::Result createResult =
        client.Post("/communities/" + std::to_string(community.id) + "/channels", headers,
                    nlohmann::json{{"name", "general"}}.dump(), "application/json");
    ASSERT_TRUE(createResult);
    EXPECT_FALSE(nlohmann::json::parse(createResult->body)["is_encrypted"].get<bool>());
}

TEST(HttpServerTest, ListMembersReturnsJoinedMember) {
    auto fixtureOpt = TestFixture::create("http-server-list-members-owner");
    if (!fixtureOpt.has_value()) {
        GTEST_SKIP() << "Postgres or auth-service not reachable — run `docker compose up` + start auth-service.";
    }
    auto& fixture = *fixtureOpt;
    const std::string memberLogin = "http-server-list-members-user-" + uniqueSuffix();
    const std::optional<std::string> memberToken =
        registerAndGetToken(fixture.authHost, fixture.authPort, memberLogin);
    ASSERT_TRUE(memberToken.has_value());

    ChatService chatService(fixture.repository);
    const Community community =
        chatService.createCommunity("http-test-members-" + uniqueSuffix(), fixture.ownerLogin);

    const ScopedServer server(chatService, fixture.authServiceClient);
    httplib::Client client(kTestHost, kTestPort);
    httplib::Headers memberHeaders{{"Authorization", bearer(*memberToken)}};
    const httplib::Result joinResult =
        client.Post("/communities/" + std::to_string(community.id) + "/join", memberHeaders, "", "application/json");
    ASSERT_TRUE(joinResult);
    ASSERT_EQ(joinResult->status, 200);

    httplib::Headers ownerHeaders{{"Authorization", bearer(fixture.ownerToken)}};
    const httplib::Result membersResult =
        client.Get("/communities/" + std::to_string(community.id) + "/members", ownerHeaders);
    ASSERT_TRUE(membersResult);
    ASSERT_EQ(membersResult->status, 200);
    const nlohmann::json members = nlohmann::json::parse(membersResult->body);
    const std::vector<std::string> memberList = members.get<std::vector<std::string>>();
    EXPECT_NE(std::find(memberList.begin(), memberList.end(), memberLogin), memberList.end());
}

TEST(HttpServerTest, SetChannelKeySucceedsForOwnerAndGetMyChannelKeyRoundTrips) {
    auto fixtureOpt = TestFixture::create("http-server-set-channel-key");
    if (!fixtureOpt.has_value()) {
        GTEST_SKIP() << "Postgres or auth-service not reachable — run `docker compose up` + start auth-service.";
    }
    auto& fixture = *fixtureOpt;
    ChatService chatService(fixture.repository);
    const Community community =
        chatService.createCommunity("http-test-set-key-" + uniqueSuffix(), fixture.ownerLogin);
    const std::optional<std::int64_t> channelId =
        chatService.createChannel(community.id, "secret", fixture.ownerLogin, /*isEncrypted=*/true);
    ASSERT_TRUE(channelId.has_value());

    const ScopedServer server(chatService, fixture.authServiceClient);
    httplib::Client client(kTestHost, kTestPort);
    httplib::Headers headers{{"Authorization", bearer(fixture.ownerToken)}};

    // Пока ничего не установлено.
    const httplib::Result beforeResult =
        client.Get("/channels/" + std::to_string(*channelId) + "/keys/me", headers);
    ASSERT_TRUE(beforeResult);
    EXPECT_EQ(beforeResult->status, 404);

    const httplib::Result setResult =
        client.Put("/channels/" + std::to_string(*channelId) + "/keys/" + fixture.ownerLogin, headers,
                   nlohmann::json{{"wrapped_key", "sealed-bytes-base64"}}.dump(), "application/json");
    ASSERT_TRUE(setResult);
    EXPECT_EQ(setResult->status, 200);

    const httplib::Result afterResult =
        client.Get("/channels/" + std::to_string(*channelId) + "/keys/me", headers);
    ASSERT_TRUE(afterResult);
    ASSERT_EQ(afterResult->status, 200);
    EXPECT_EQ(nlohmann::json::parse(afterResult->body)["wrapped_key"].get<std::string>(), "sealed-bytes-base64");
}

TEST(HttpServerTest, SetChannelKeyRejectsNonOwnerNonModeratorWith403) {
    auto fixtureOpt = TestFixture::create("http-server-set-key-forbidden-owner");
    if (!fixtureOpt.has_value()) {
        GTEST_SKIP() << "Postgres or auth-service not reachable — run `docker compose up` + start auth-service.";
    }
    auto& fixture = *fixtureOpt;
    const std::optional<std::string> outsiderToken =
        registerAndGetToken(fixture.authHost, fixture.authPort, "http-server-set-key-outsider-" + uniqueSuffix());
    ASSERT_TRUE(outsiderToken.has_value());

    ChatService chatService(fixture.repository);
    const Community community =
        chatService.createCommunity("http-test-set-key-403-" + uniqueSuffix(), fixture.ownerLogin);
    const std::optional<std::int64_t> channelId =
        chatService.createChannel(community.id, "secret", fixture.ownerLogin, /*isEncrypted=*/true);
    ASSERT_TRUE(channelId.has_value());

    const ScopedServer server(chatService, fixture.authServiceClient);
    httplib::Client client(kTestHost, kTestPort);
    httplib::Headers outsiderHeaders{{"Authorization", bearer(*outsiderToken)}};
    const httplib::Result result =
        client.Put("/channels/" + std::to_string(*channelId) + "/keys/" + fixture.ownerLogin, outsiderHeaders,
                   nlohmann::json{{"wrapped_key", "forged"}}.dump(), "application/json");

    ASSERT_TRUE(result);
    EXPECT_EQ(result->status, 403);
}

TEST(HttpServerTest, SearchMessagesRejectsEncryptedChannelWith400) {
    auto fixtureOpt = TestFixture::create("http-server-search-encrypted");
    if (!fixtureOpt.has_value()) {
        GTEST_SKIP() << "Postgres or auth-service not reachable — run `docker compose up` + start auth-service.";
    }
    auto& fixture = *fixtureOpt;
    ChatService chatService(fixture.repository);
    const Community community =
        chatService.createCommunity("http-test-search-enc-" + uniqueSuffix(), fixture.ownerLogin);
    const std::optional<std::int64_t> channelId =
        chatService.createChannel(community.id, "secret", fixture.ownerLogin, /*isEncrypted=*/true);
    ASSERT_TRUE(channelId.has_value());

    const ScopedServer server(chatService, fixture.authServiceClient);
    httplib::Client client(kTestHost, kTestPort);
    httplib::Headers headers{{"Authorization", bearer(fixture.ownerToken)}};
    const httplib::Result result =
        client.Get("/channels/" + std::to_string(*channelId) + "/messages/search?q=anything", headers);

    ASSERT_TRUE(result);
    EXPECT_EQ(result->status, 400);
}

TEST(HttpServerTest, UploadAttachmentRejectsEncryptedChannelWith400) {
    auto fixtureOpt = TestFixture::create("http-server-upload-encrypted");
    if (!fixtureOpt.has_value()) {
        GTEST_SKIP() << "Postgres or auth-service not reachable — run `docker compose up` + start auth-service.";
    }
    auto& fixture = *fixtureOpt;
    ChatService chatService(fixture.repository);
    const Community community =
        chatService.createCommunity("http-test-upload-enc-" + uniqueSuffix(), fixture.ownerLogin);
    const std::optional<std::int64_t> channelId =
        chatService.createChannel(community.id, "secret", fixture.ownerLogin, /*isEncrypted=*/true);
    ASSERT_TRUE(channelId.has_value());

    const ScopedServer server(chatService, fixture.authServiceClient);
    httplib::Client client(kTestHost, kTestPort);
    httplib::Headers headers{{"Authorization", bearer(fixture.ownerToken)}};
    const httplib::Result result = client.Post(
        "/channels/" + std::to_string(*channelId) + "/attachments", headers,
        nlohmann::json{{"filename", "x.txt"}, {"content_type", "text/plain"}, {"data_base64", "SGVsbG8="}}.dump(),
        "application/json");

    ASSERT_TRUE(result);
    EXPECT_EQ(result->status, 400);
}

}  // namespace
}  // namespace chat_service
