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

// Route-level tests for HttpServer, hitting a real httplib::Server
// instance over loopback HTTP. Requires a live Postgres (chat-service's
// own, see docker-compose.yml) AND a live auth-service (to mint real
// tokens for the Authorization header) — same dependencies as
// WebSocketServerTest.cpp. Skips itself rather than failing when either
// isn't reachable.

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

// Same helper as WebSocketServerTest.cpp: registers a brand-new user
// against a live auth-service and returns its token.
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

// Bundles everything a test needs: repository/auth client, a real owner
// token — GTEST_SKIPs the calling test if either Postgres or
// auth-service isn't reachable. Deliberately does NOT hold a ChatService
// member: ChatService stores a reference to its repository, and this
// struct is returned by value through std::optional (a move), which
// would leave that reference dangling — each test constructs its own
// local ChatService from fixture.repository instead.
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

    // 7,000,000 'A' characters decode cleanly (divisible by 4) to
    // 5,250,000 zero bytes — over the 5 MB (5,242,880-byte) cap.
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

    // "Hello, attachment!" base64-encoded.
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

}  // namespace
}  // namespace chat_service
