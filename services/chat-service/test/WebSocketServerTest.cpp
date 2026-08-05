#include "AuthServiceClient.h"
#include "ChatRepository.h"
#include "ChatService.h"
#include "WebSocketServer.h"

#include <gtest/gtest.h>

#include <httplib.h>
#include <ixwebsocket/IXNetSystem.h>
#include <ixwebsocket/IXWebSocket.h>
#include <nlohmann/json.hpp>

#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <deque>
#include <functional>
#include <mutex>
#include <optional>
#include <string>

// Requires a live Postgres (see docker-compose.yml, chat-service-postgres)
// AND a live auth-service (to mint real tokens for the hello handshake).
// Skips itself rather than failing when either isn't reachable — see
// ChatServiceIntegrationTest.cpp for the same pattern.

namespace chat_service {
namespace {

std::string envOrDefault(const char* name, const std::string& defaultValue) {
    const char* value = std::getenv(name);
    return value != nullptr ? std::string(value) : defaultValue;
}

std::string uniqueSuffix() {
    return std::to_string(
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
            .count());
}

/// Registers a brand-new user against a live auth-service and returns
/// its token, or std::nullopt if auth-service isn't reachable.
std::optional<std::string> registerAndGetToken(const std::string& host, int port, const std::string& login) {
    httplib::Client client(host, port);
    const nlohmann::json body{{"login", login}, {"password", "ws-test-password"}};
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

/// Minimal synchronous test client over ix::WebSocket — queues every
/// received JSON frame so a test can wait for the one it expects
/// without racing ixwebsocket's own callback thread.
class WsTestClient {
public:
    explicit WsTestClient(const std::string& url) {
        socket_.setUrl(url);
        socket_.setOnMessageCallback([this](const ix::WebSocketMessagePtr& message) {
            if (message->type == ix::WebSocketMessageType::Open) {
                const std::lock_guard<std::mutex> lock(mutex_);
                connected_ = true;
                cv_.notify_all();
                return;
            }
            if (message->type != ix::WebSocketMessageType::Message) {
                return;
            }
            const nlohmann::json parsed = nlohmann::json::parse(message->str, nullptr, /*allow_exceptions=*/false);
            if (parsed.is_discarded()) {
                return;
            }
            const std::lock_guard<std::mutex> lock(mutex_);
            queue_.push_back(parsed);
            cv_.notify_all();
        });
        socket_.start();
    }

    ~WsTestClient() { socket_.stop(); }

    /// Blocks until the connection's Open event fires (or times out).
    [[nodiscard]] bool waitConnected(int timeoutMs = 3000) {
        std::unique_lock<std::mutex> lock(mutex_);
        return cv_.wait_for(lock, std::chrono::milliseconds(timeoutMs), [this] { return connected_; });
    }

    void send(const nlohmann::json& frame) { socket_.sendText(frame.dump()); }

    /// Waits up to @p timeoutMs for a queued message matching @p predicate,
    /// consuming and returning it; std::nullopt on timeout.
    std::optional<nlohmann::json> waitFor(const std::function<bool(const nlohmann::json&)>& predicate,
                                           int timeoutMs = 3000) {
        std::unique_lock<std::mutex> lock(mutex_);
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
        while (true) {
            for (auto it = queue_.begin(); it != queue_.end(); ++it) {
                if (predicate(*it)) {
                    nlohmann::json found = *it;
                    queue_.erase(it);
                    return found;
                }
            }
            if (cv_.wait_until(lock, deadline) == std::cv_status::timeout) {
                return std::nullopt;
            }
        }
    }

private:
    ix::WebSocket socket_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::deque<nlohmann::json> queue_;
    bool connected_ = false;
};

}  // namespace

TEST(WebSocketServerTest, CallJoinRosterSignalAndLeave) {
    ix::initNetSystem();

    const std::string dbConnectionString = envOrDefault(
        "CHAT_SERVICE_DATABASE_URL", "postgresql://chat_service:dev-only-password@localhost:5434/chat_service");
    const std::string authHost = envOrDefault("AUTH_SERVICE_HOST", "127.0.0.1");
    const int authPort = std::stoi(envOrDefault("AUTH_SERVICE_PORT", "8080"));
    const int wsPort = std::stoi(envOrDefault("CHAT_SERVICE_TEST_WS_PORT", "18083"));

    ChatRepository repository(dbConnectionString);
    ChatService service(repository);

    const std::string suffix = uniqueSuffix();
    const std::string owner = "ws-test-owner-" + suffix;
    Community community{};
    try {
        community = service.createCommunity("ws-test-community-" + suffix, owner);
    } catch (const std::exception& error) {
        GTEST_SKIP() << "Postgres not reachable (" << error.what() << ") — run `docker compose up` to run this test.";
    }
    const std::optional<std::int64_t> channelId = service.createChannel(community.id, "general", owner);
    ASSERT_TRUE(channelId.has_value());

    const std::string loginA = "ws-test-a-" + suffix;
    const std::string loginB = "ws-test-b-" + suffix;
    const std::optional<std::string> tokenA = registerAndGetToken(authHost, authPort, loginA);
    if (!tokenA.has_value()) {
        GTEST_SKIP() << "auth-service not reachable — start it locally to run this test.";
    }
    const std::optional<std::string> tokenB = registerAndGetToken(authHost, authPort, loginB);
    ASSERT_TRUE(tokenB.has_value());

    AuthServiceClient authServiceClient(authHost, authPort);
    WebSocketServer server(service, authServiceClient, wsPort);
    ASSERT_TRUE(server.start());

    const std::string wsUrl = "ws://127.0.0.1:" + std::to_string(wsPort) + "/";
    WsTestClient clientA(wsUrl);
    WsTestClient clientB(wsUrl);
    ASSERT_TRUE(clientA.waitConnected());
    ASSERT_TRUE(clientB.waitConnected());

    clientA.send(nlohmann::json{{"token", *tokenA}, {"channel_id", *channelId}});
    ASSERT_TRUE(clientA.waitFor([](const nlohmann::json& m) { return m.contains("subscribed"); }).has_value());

    clientB.send(nlohmann::json{{"token", *tokenB}, {"channel_id", *channelId}});
    ASSERT_TRUE(clientB.waitFor([](const nlohmann::json& m) { return m.contains("subscribed"); }).has_value());

    // A joins the call first — sees an empty roster (no one else there yet).
    clientA.send(nlohmann::json{{"call_join", true}});
    const std::optional<nlohmann::json> rosterA =
        clientA.waitFor([](const nlohmann::json& m) { return m.contains("call_roster"); });
    ASSERT_TRUE(rosterA.has_value());
    EXPECT_TRUE((*rosterA)["call_roster"].empty());

    // B joins second — sees A in its roster, and A gets notified of B.
    clientB.send(nlohmann::json{{"call_join", true}});
    const std::optional<nlohmann::json> rosterB =
        clientB.waitFor([](const nlohmann::json& m) { return m.contains("call_roster"); });
    ASSERT_TRUE(rosterB.has_value());
    ASSERT_EQ((*rosterB)["call_roster"].size(), 1U);
    EXPECT_EQ((*rosterB)["call_roster"][0].get<std::string>(), loginA);

    const std::optional<nlohmann::json> peerJoined =
        clientA.waitFor([](const nlohmann::json& m) { return m.contains("call_peer_joined"); });
    ASSERT_TRUE(peerJoined.has_value());
    EXPECT_EQ((*peerJoined)["call_peer_joined"].get<std::string>(), loginB);

    // Mesh rule: the newer joiner (B) offers to the peer already present (A).
    clientB.send(nlohmann::json{
        {"call_signal", {{"to", loginA}, {"payload", {{"kind", "offer"}, {"sdp", "fake-sdp"}}}}}});
    const std::optional<nlohmann::json> signalOnA =
        clientA.waitFor([](const nlohmann::json& m) { return m.contains("call_signal"); });
    ASSERT_TRUE(signalOnA.has_value());
    EXPECT_EQ((*signalOnA)["call_signal"]["from"].get<std::string>(), loginB);
    EXPECT_EQ((*signalOnA)["call_signal"]["payload"]["sdp"].get<std::string>(), "fake-sdp");

    // Signaling to someone not in the call is rejected, not silently dropped.
    clientA.send(
        nlohmann::json{{"call_signal", {{"to", "nobody-" + suffix}, {"payload", nlohmann::json::object()}}}});
    EXPECT_TRUE(clientA.waitFor([](const nlohmann::json& m) { return m.contains("error"); }).has_value());

    // B leaves — A is notified.
    clientB.send(nlohmann::json{{"call_leave", true}});
    const std::optional<nlohmann::json> peerLeft =
        clientA.waitFor([](const nlohmann::json& m) { return m.contains("call_peer_left"); });
    ASSERT_TRUE(peerLeft.has_value());
    EXPECT_EQ((*peerLeft)["call_peer_left"].get<std::string>(), loginB);

    server.stop();
}

}  // namespace chat_service
