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
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

// Требует работающий Postgres (см. docker-compose.yml,
// chat-service-postgres) И работающий auth-service (для выпуска
// настоящих токенов для hello-рукопожатия). Пропускает себя, а не
// падает, если что-то из этого недоступно — см. ChatServiceIntegrationTest.cpp
// за тем же паттерном.

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

/// Регистрирует нового пользователя в работающем auth-service и
/// возвращает его токен, либо std::nullopt, если auth-service недоступен.
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

/// Минимальный синхронный тестовый клиент поверх ix::WebSocket —
/// ставит в очередь каждый полученный JSON-кадр, чтобы тест мог ждать
/// именно тот, который ожидает, не гоняясь наперегонки с собственным
/// потоком колбэков ixwebsocket.
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

    /// Блокируется до срабатывания события Open соединения (или до тайм-аута).
    [[nodiscard]] bool waitConnected(int timeoutMs = 3000) {
        std::unique_lock<std::mutex> lock(mutex_);
        return cv_.wait_for(lock, std::chrono::milliseconds(timeoutMs), [this] { return connected_; });
    }

    void send(const nlohmann::json& frame) { socket_.sendText(frame.dump()); }

    /// Отправляет сырой текстовый кадр, минуя nlohmann::json::dump() —
    /// клиент реального атакующего не обязательно построен на nlohmann,
    /// так что это то, что действительно имитирует враждебные, но
    /// синтаксически валидные JSON-байты, доходящие до собственного
    /// nlohmann::json::parse() сервера без изменений.
    void sendRaw(const std::string& text) { socket_.sendText(text); }

    /// Ждёт до @p timeoutMs поставленное в очередь сообщение,
    /// соответствующее @p predicate, забирая и возвращая его;
    /// std::nullopt по истечении тайм-аута.
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

    // A присоединяется к звонку первым — видит пустой список участников (больше пока никого нет).
    clientA.send(nlohmann::json{{"call_join", true}});
    const std::optional<nlohmann::json> rosterA =
        clientA.waitFor([](const nlohmann::json& m) { return m.contains("call_roster"); });
    ASSERT_TRUE(rosterA.has_value());
    EXPECT_TRUE((*rosterA)["call_roster"].empty());

    // B присоединяется вторым — видит A в своём списке участников, а A получает уведомление о B.
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

    // Правило mesh-топологии: тот, кто присоединился позже (B), делает offer уже присутствующему участнику (A).
    clientB.send(nlohmann::json{
        {"call_signal", {{"to", loginA}, {"payload", {{"kind", "offer"}, {"sdp", "fake-sdp"}}}}}});
    const std::optional<nlohmann::json> signalOnA =
        clientA.waitFor([](const nlohmann::json& m) { return m.contains("call_signal"); });
    ASSERT_TRUE(signalOnA.has_value());
    EXPECT_EQ((*signalOnA)["call_signal"]["from"].get<std::string>(), loginB);
    EXPECT_EQ((*signalOnA)["call_signal"]["payload"]["sdp"].get<std::string>(), "fake-sdp");

    // Сигналинг тому, кто не в звонке, отклоняется, а не молча отбрасывается.
    clientA.send(
        nlohmann::json{{"call_signal", {{"to", "nobody-" + suffix}, {"payload", nlohmann::json::object()}}}});
    EXPECT_TRUE(clientA.waitFor([](const nlohmann::json& m) { return m.contains("error"); }).has_value());

    // B покидает звонок — A получает уведомление.
    clientB.send(nlohmann::json{{"call_leave", true}});
    const std::optional<nlohmann::json> peerLeft =
        clientA.waitFor([](const nlohmann::json& m) { return m.contains("call_peer_left"); });
    ASSERT_TRUE(peerLeft.has_value());
    EXPECT_EQ((*peerLeft)["call_peer_left"].get<std::string>(), loginB);

    server.stop();
}

TEST(WebSocketServerTest, HelloWithMissingFieldsIsRejectedWithError) {
    ix::initNetSystem();

    const std::string dbConnectionString = envOrDefault(
        "CHAT_SERVICE_DATABASE_URL", "postgresql://chat_service:dev-only-password@localhost:5434/chat_service");
    const std::string authHost = envOrDefault("AUTH_SERVICE_HOST", "127.0.0.1");
    const int authPort = std::stoi(envOrDefault("AUTH_SERVICE_PORT", "8080"));
    const int wsPort = std::stoi(envOrDefault("CHAT_SERVICE_TEST_WS_PORT", "18084"));

    ChatRepository repository(dbConnectionString);
    ChatService service(repository);
    const std::string suffix = uniqueSuffix();

    // Проверка доступности, тот же паттерн, что и в CallJoinRosterSignalAndLeave.
    try {
        static_cast<void>(service.createCommunity("ws-hello-test-probe-" + suffix, "ws-hello-test-owner-" + suffix));
    } catch (const std::exception& error) {
        GTEST_SKIP() << "Postgres not reachable (" << error.what() << ") — run `docker compose up` to run this test.";
    }

    AuthServiceClient authServiceClient(authHost, authPort);
    WebSocketServer server(service, authServiceClient, wsPort);
    ASSERT_TRUE(server.start());

    WsTestClient client("ws://127.0.0.1:" + std::to_string(wsPort) + "/");
    ASSERT_TRUE(client.waitConnected());

    // Отсутствует "channel_id".
    client.send(nlohmann::json{{"token", "irrelevant"}});
    const std::optional<nlohmann::json> error =
        client.waitFor([](const nlohmann::json& m) { return m.contains("error"); });
    ASSERT_TRUE(error.has_value());

    server.stop();
}

TEST(WebSocketServerTest, HelloWithInvalidTokenIsRejectedWithError) {
    ix::initNetSystem();

    const std::string dbConnectionString = envOrDefault(
        "CHAT_SERVICE_DATABASE_URL", "postgresql://chat_service:dev-only-password@localhost:5434/chat_service");
    const std::string authHost = envOrDefault("AUTH_SERVICE_HOST", "127.0.0.1");
    const int authPort = std::stoi(envOrDefault("AUTH_SERVICE_PORT", "8080"));
    const int wsPort = std::stoi(envOrDefault("CHAT_SERVICE_TEST_WS_PORT", "18085"));

    ChatRepository repository(dbConnectionString);
    ChatService service(repository);
    const std::string suffix = uniqueSuffix();
    const std::string owner = "ws-badtoken-test-owner-" + suffix;
    Community community{};
    try {
        community = service.createCommunity("ws-badtoken-test-community-" + suffix, owner);
    } catch (const std::exception& error) {
        GTEST_SKIP() << "Postgres not reachable (" << error.what() << ") — run `docker compose up` to run this test.";
    }
    const std::optional<std::int64_t> channelId = service.createChannel(community.id, "general", owner);
    ASSERT_TRUE(channelId.has_value());

    AuthServiceClient authServiceClient(authHost, authPort);
    WebSocketServer server(service, authServiceClient, wsPort);
    ASSERT_TRUE(server.start());

    WsTestClient client("ws://127.0.0.1:" + std::to_string(wsPort) + "/");
    ASSERT_TRUE(client.waitConnected());

    client.send(nlohmann::json{{"token", "not-a-real-token"}, {"channel_id", *channelId}});
    const std::optional<nlohmann::json> error =
        client.waitFor([](const nlohmann::json& m) { return m.contains("error"); });
    ASSERT_TRUE(error.has_value());
    EXPECT_EQ((*error)["error"].get<std::string>(), "invalid token");

    server.stop();
}

// Временная замена настоящему fuzzing'у, управляемому покрытием (issue
// #121 — libFuzzer требует clang-cl, недоступный в тулчейне MSVC этого
// окружения): батарея враждебных, но синтаксически разбираемых кадров —
// неверные типы полей, переполнение int64_t, вложенные/не-объектные
// payload'ы, пустые строки — нацеленных на управляемую клиентом
// диспетчеризацию в handleSubscribedMessage() (issue #46 и последующие).
// Проверяемое свойство — что *процесс* остаётся живым и отвечающим:
// неперехваченное исключение nlohmann::json, вырвавшееся из колбэка
// сообщений ix::WebSocket, вызвало бы std::terminate всего chat-service,
// утянув за собой всех остальных подключённых клиентов, так что пережить
// их все и всё ещё ответить на финальный корректно сформированный кадр —
// вот что здесь значит "пройден"; от каждого отдельного кадра успех не ожидается.
TEST(WebSocketServerTest, SubscribedDispatchSurvivesAdversarialPayloads) {
    ix::initNetSystem();

    const std::string dbConnectionString = envOrDefault(
        "CHAT_SERVICE_DATABASE_URL", "postgresql://chat_service:dev-only-password@localhost:5434/chat_service");
    const std::string authHost = envOrDefault("AUTH_SERVICE_HOST", "127.0.0.1");
    const int authPort = std::stoi(envOrDefault("AUTH_SERVICE_PORT", "8080"));
    const int wsPort = std::stoi(envOrDefault("CHAT_SERVICE_TEST_WS_PORT", "18090"));

    ChatRepository repository(dbConnectionString);
    ChatService service(repository);
    const std::string suffix = uniqueSuffix();
    const std::string owner = "ws-adversarial-test-owner-" + suffix;
    Community community{};
    try {
        community = service.createCommunity("ws-adversarial-test-community-" + suffix, owner);
    } catch (const std::exception& error) {
        GTEST_SKIP() << "Postgres not reachable (" << error.what() << ") — run `docker compose up` to run this test.";
    }
    const std::optional<std::int64_t> channelId = service.createChannel(community.id, "general", owner);
    ASSERT_TRUE(channelId.has_value());

    const std::string login = "ws-adversarial-" + suffix;
    const std::optional<std::string> token = registerAndGetToken(authHost, authPort, login);
    if (!token.has_value()) {
        GTEST_SKIP() << "auth-service not reachable — start it locally to run this test.";
    }

    AuthServiceClient authServiceClient(authHost, authPort);
    WebSocketServer server(service, authServiceClient, wsPort);
    ASSERT_TRUE(server.start());

    WsTestClient client("ws://127.0.0.1:" + std::to_string(wsPort) + "/");
    ASSERT_TRUE(client.waitConnected());

    client.send(nlohmann::json{{"token", *token}, {"channel_id", *channelId}});
    ASSERT_TRUE(client.waitFor([](const nlohmann::json& m) { return m.contains("subscribed"); }).has_value());

    const std::vector<nlohmann::json> adversarialFrames = {
        nlohmann::json(42),
        nlohmann::json("just a string, not an object"),
        nlohmann::json::array({1, 2, 3}),
        nlohmann::json{{"call_signal", 12345}},
        nlohmann::json{{"call_signal", "not an object"}},
        nlohmann::json{{"call_signal", nlohmann::json::array()}},
        nlohmann::json{{"call_signal", {{"to", 999}, {"payload", "x"}}}},
        nlohmann::json{{"call_signal", {{"to", ""}, {"payload", nullptr}}}},
        nlohmann::json{{"edit_message", "not an object"}},
        nlohmann::json{{"edit_message", {{"id", "not-a-number"}, {"body", "x"}}}},
        // int64_t::max() + 1 — is_number_integer() истинно (разбирается
        // как number_unsigned), но get<int64_t>() может бросить исключение при переполнении.
        nlohmann::json{{"edit_message", {{"id", 9223372036854775808ULL}, {"body", "x"}}}},
        nlohmann::json{{"delete_message", nlohmann::json::array()}},
        nlohmann::json{{"delete_message", {{"id", -9223372036854775807LL - 1}}}},
        nlohmann::json{{"body", nullptr}},
        nlohmann::json{{"body", 12345}},
        nlohmann::json{{"body", ""}},
        nlohmann::json{{"body", "x"}, {"attachment_id", "not-a-number"}},
        nlohmann::json{{"body", "x"}, {"attachment_id", nlohmann::json::array()}},
        nlohmann::json{{"typing", nlohmann::json::object()}},
        nlohmann::json{{"typing", nullptr}},
    };
    for (const nlohmann::json& frame : adversarialFrames) {
        client.send(frame);
    }

    // Сильно вложенный сырой payload — проверяет защиту от переполнения
    // стека рекурсивным спуском nlohmann::json::parse() на глубине
    // вложенности, контролируемой атакующим. Построен как сырая строка
    // (не через nlohmann::json::dump(), который рекурсирует так же
    // глубоко на *отправляющей* стороне), поскольку реальный атакующий
    // не обязательно вообще отправляет байты, произведённые nlohmann.
    std::string deeplyNestedFrame(500, '[');
    deeplyNestedFrame.append(500, ']');
    client.sendRaw(deeplyNestedFrame);

    // Позитивная проверка живости: если процесс пережил всё вышеперечисленное,
    // корректно сформированное сообщение чата, отправленное после этого,
    // всё ещё рассылается обратно (тот же round trip, что и в
    // ChatMessageIsBroadcastToAllSubscribersIncludingSender).
    client.send(nlohmann::json{{"body", "still alive after the adversarial batch"}});
    const std::optional<nlohmann::json> echo = client.waitFor([](const nlohmann::json& m) {
        return m.contains("body") && m["body"] == "still alive after the adversarial batch";
    });
    ASSERT_TRUE(echo.has_value());

    server.stop();
}

TEST(WebSocketServerTest, ChatMessageIsBroadcastToAllSubscribersIncludingSender) {
    ix::initNetSystem();

    const std::string dbConnectionString = envOrDefault(
        "CHAT_SERVICE_DATABASE_URL", "postgresql://chat_service:dev-only-password@localhost:5434/chat_service");
    const std::string authHost = envOrDefault("AUTH_SERVICE_HOST", "127.0.0.1");
    const int authPort = std::stoi(envOrDefault("AUTH_SERVICE_PORT", "8080"));
    const int wsPort = std::stoi(envOrDefault("CHAT_SERVICE_TEST_WS_PORT", "18086"));

    ChatRepository repository(dbConnectionString);
    ChatService service(repository);
    const std::string suffix = uniqueSuffix();
    const std::string owner = "ws-chatmsg-test-owner-" + suffix;
    Community community{};
    try {
        community = service.createCommunity("ws-chatmsg-test-community-" + suffix, owner);
    } catch (const std::exception& error) {
        GTEST_SKIP() << "Postgres not reachable (" << error.what() << ") — run `docker compose up` to run this test.";
    }
    const std::optional<std::int64_t> channelId = service.createChannel(community.id, "general", owner);
    ASSERT_TRUE(channelId.has_value());

    const std::string loginA = "ws-chatmsg-a-" + suffix;
    const std::string loginB = "ws-chatmsg-b-" + suffix;
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

    clientA.send(nlohmann::json{{"body", "hello from A"}});

    const std::optional<nlohmann::json> onA =
        clientA.waitFor([](const nlohmann::json& m) { return m.contains("body"); });
    ASSERT_TRUE(onA.has_value());
    EXPECT_EQ((*onA)["author"].get<std::string>(), loginA);
    EXPECT_EQ((*onA)["body"].get<std::string>(), "hello from A");

    const std::optional<nlohmann::json> onB =
        clientB.waitFor([](const nlohmann::json& m) { return m.contains("body"); });
    ASSERT_TRUE(onB.has_value());
    EXPECT_EQ((*onB)["author"].get<std::string>(), loginA);
    EXPECT_EQ((*onB)["body"].get<std::string>(), "hello from A");

    server.stop();
}

TEST(WebSocketServerTest, ChatMessageWithMissingBodyReturnsError) {
    ix::initNetSystem();

    const std::string dbConnectionString = envOrDefault(
        "CHAT_SERVICE_DATABASE_URL", "postgresql://chat_service:dev-only-password@localhost:5434/chat_service");
    const std::string authHost = envOrDefault("AUTH_SERVICE_HOST", "127.0.0.1");
    const int authPort = std::stoi(envOrDefault("AUTH_SERVICE_PORT", "8080"));
    const int wsPort = std::stoi(envOrDefault("CHAT_SERVICE_TEST_WS_PORT", "18087"));

    ChatRepository repository(dbConnectionString);
    ChatService service(repository);
    const std::string suffix = uniqueSuffix();
    const std::string owner = "ws-badbody-test-owner-" + suffix;
    Community community{};
    try {
        community = service.createCommunity("ws-badbody-test-community-" + suffix, owner);
    } catch (const std::exception& error) {
        GTEST_SKIP() << "Postgres not reachable (" << error.what() << ") — run `docker compose up` to run this test.";
    }
    const std::optional<std::int64_t> channelId = service.createChannel(community.id, "general", owner);
    ASSERT_TRUE(channelId.has_value());

    const std::optional<std::string> token = registerAndGetToken(authHost, authPort, owner);
    if (!token.has_value()) {
        GTEST_SKIP() << "auth-service not reachable — start it locally to run this test.";
    }

    AuthServiceClient authServiceClient(authHost, authPort);
    WebSocketServer server(service, authServiceClient, wsPort);
    ASSERT_TRUE(server.start());

    WsTestClient client("ws://127.0.0.1:" + std::to_string(wsPort) + "/");
    ASSERT_TRUE(client.waitConnected());
    client.send(nlohmann::json{{"token", *token}, {"channel_id", *channelId}});
    ASSERT_TRUE(client.waitFor([](const nlohmann::json& m) { return m.contains("subscribed"); }).has_value());

    client.send(nlohmann::json::object());
    EXPECT_TRUE(client.waitFor([](const nlohmann::json& m) { return m.contains("error"); }).has_value());

    server.stop();
}

TEST(WebSocketServerTest, ModeratorCanDeleteAnotherSubscribersMessageButNotEditIt) {
    ix::initNetSystem();

    const std::string dbConnectionString = envOrDefault(
        "CHAT_SERVICE_DATABASE_URL", "postgresql://chat_service:dev-only-password@localhost:5434/chat_service");
    const std::string authHost = envOrDefault("AUTH_SERVICE_HOST", "127.0.0.1");
    const int authPort = std::stoi(envOrDefault("AUTH_SERVICE_PORT", "8080"));
    const int wsPort = std::stoi(envOrDefault("CHAT_SERVICE_TEST_WS_PORT", "18089"));

    ChatRepository repository(dbConnectionString);
    ChatService service(repository);
    const std::string suffix = uniqueSuffix();
    const std::string owner = "ws-mod-test-owner-" + suffix;
    Community community{};
    try {
        community = service.createCommunity("ws-mod-test-community-" + suffix, owner);
    } catch (const std::exception& error) {
        GTEST_SKIP() << "Postgres not reachable (" << error.what() << ") — run `docker compose up` to run this test.";
    }
    const std::optional<std::int64_t> channelId = service.createChannel(community.id, "general", owner);
    ASSERT_TRUE(channelId.has_value());

    const std::string authorLogin = "ws-mod-test-author-" + suffix;
    const std::string moderatorLogin = "ws-mod-test-mod-" + suffix;
    const std::optional<std::string> authorToken = registerAndGetToken(authHost, authPort, authorLogin);
    if (!authorToken.has_value()) {
        GTEST_SKIP() << "auth-service not reachable — start it locally to run this test.";
    }
    const std::optional<std::string> moderatorToken = registerAndGetToken(authHost, authPort, moderatorLogin);
    ASSERT_TRUE(moderatorToken.has_value());
    ASSERT_EQ(service.promoteModerator(community.id, moderatorLogin, owner), MutationResult::kSuccess);

    AuthServiceClient authServiceClient(authHost, authPort);
    WebSocketServer server(service, authServiceClient, wsPort);
    ASSERT_TRUE(server.start());

    const std::string wsUrl = "ws://127.0.0.1:" + std::to_string(wsPort) + "/";
    WsTestClient author(wsUrl);
    WsTestClient moderator(wsUrl);
    ASSERT_TRUE(author.waitConnected());
    ASSERT_TRUE(moderator.waitConnected());
    author.send(nlohmann::json{{"token", *authorToken}, {"channel_id", *channelId}});
    ASSERT_TRUE(author.waitFor([](const nlohmann::json& m) { return m.contains("subscribed"); }).has_value());
    moderator.send(nlohmann::json{{"token", *moderatorToken}, {"channel_id", *channelId}});
    ASSERT_TRUE(moderator.waitFor([](const nlohmann::json& m) { return m.contains("subscribed"); }).has_value());

    author.send(nlohmann::json{{"body", "author's message"}});
    const std::optional<nlohmann::json> posted =
        moderator.waitFor([](const nlohmann::json& m) { return m.contains("body"); });
    ASSERT_TRUE(posted.has_value());
    const auto messageId = (*posted)["id"].get<std::int64_t>();

    // Модератор не может её редактировать — то же правило "только автор",
    // что и до issue #114.
    moderator.send(nlohmann::json{{"edit_message", {{"id", messageId}, {"body", "hijacked"}}}});
    ASSERT_TRUE(moderator.waitFor([](const nlohmann::json& m) { return m.contains("error"); }).has_value());

    // Но модератор МОЖЕТ его удалить — рассылка каждому подписчику,
    // включая исходного автора.
    moderator.send(nlohmann::json{{"delete_message", {{"id", messageId}}}});
    const std::optional<nlohmann::json> deletedOnAuthor =
        author.waitFor([](const nlohmann::json& m) { return m.contains("message_deleted"); });
    ASSERT_TRUE(deletedOnAuthor.has_value());
    EXPECT_EQ((*deletedOnAuthor)["message_deleted"]["id"].get<std::int64_t>(), messageId);

    server.stop();
}

TEST(WebSocketServerTest, ChatMessageWithAttachmentIdBroadcastsAttachmentFields) {
    ix::initNetSystem();

    const std::string dbConnectionString = envOrDefault(
        "CHAT_SERVICE_DATABASE_URL", "postgresql://chat_service:dev-only-password@localhost:5434/chat_service");
    const std::string authHost = envOrDefault("AUTH_SERVICE_HOST", "127.0.0.1");
    const int authPort = std::stoi(envOrDefault("AUTH_SERVICE_PORT", "8080"));
    const int wsPort = std::stoi(envOrDefault("CHAT_SERVICE_TEST_WS_PORT", "18089"));

    ChatRepository repository(dbConnectionString);
    ChatService service(repository);
    const std::string suffix = uniqueSuffix();
    const std::string owner = "ws-attachment-test-owner-" + suffix;
    Community community{};
    try {
        community = service.createCommunity("ws-attachment-test-community-" + suffix, owner);
    } catch (const std::exception& error) {
        GTEST_SKIP() << "Postgres not reachable (" << error.what() << ") — run `docker compose up` to run this test.";
    }
    const std::optional<std::int64_t> channelId = service.createChannel(community.id, "general", owner);
    ASSERT_TRUE(channelId.has_value());

    const std::optional<AttachmentMetadata> attachment = service.createAttachment(
        *channelId, AttachmentUpload{.uploaderLogin = owner,
                                      .filename = "photo.png",
                                      .contentType = "image/png",
                                      .dataBase64 = "SGVsbG8=",
                                      .sizeBytes = 5});
    ASSERT_TRUE(attachment.has_value());

    const std::optional<std::string> token = registerAndGetToken(authHost, authPort, owner);
    if (!token.has_value()) {
        GTEST_SKIP() << "auth-service not reachable — start it locally to run this test.";
    }

    AuthServiceClient authServiceClient(authHost, authPort);
    WebSocketServer server(service, authServiceClient, wsPort);
    ASSERT_TRUE(server.start());

    WsTestClient client("ws://127.0.0.1:" + std::to_string(wsPort) + "/");
    ASSERT_TRUE(client.waitConnected());
    client.send(nlohmann::json{{"token", *token}, {"channel_id", *channelId}});
    ASSERT_TRUE(client.waitFor([](const nlohmann::json& m) { return m.contains("subscribed"); }).has_value());

    client.send(nlohmann::json{{"body", "check this out"}, {"attachment_id", attachment->id}});
    const std::optional<nlohmann::json> received =
        client.waitFor([](const nlohmann::json& m) { return m.contains("body"); });
    ASSERT_TRUE(received.has_value());
    EXPECT_EQ((*received)["attachment_id"].get<std::int64_t>(), attachment->id);
    EXPECT_EQ((*received)["attachment_filename"].get<std::string>(), "photo.png");

    server.stop();
}

TEST(WebSocketServerTest, DisconnectWithoutLeaveNotifiesRemainingCallParticipants) {
    ix::initNetSystem();

    const std::string dbConnectionString = envOrDefault(
        "CHAT_SERVICE_DATABASE_URL", "postgresql://chat_service:dev-only-password@localhost:5434/chat_service");
    const std::string authHost = envOrDefault("AUTH_SERVICE_HOST", "127.0.0.1");
    const int authPort = std::stoi(envOrDefault("AUTH_SERVICE_PORT", "8080"));
    const int wsPort = std::stoi(envOrDefault("CHAT_SERVICE_TEST_WS_PORT", "18088"));

    ChatRepository repository(dbConnectionString);
    ChatService service(repository);
    const std::string suffix = uniqueSuffix();
    const std::string owner = "ws-disconnect-test-owner-" + suffix;
    Community community{};
    try {
        community = service.createCommunity("ws-disconnect-test-community-" + suffix, owner);
    } catch (const std::exception& error) {
        GTEST_SKIP() << "Postgres not reachable (" << error.what() << ") — run `docker compose up` to run this test.";
    }
    const std::optional<std::int64_t> channelId = service.createChannel(community.id, "general", owner);
    ASSERT_TRUE(channelId.has_value());

    const std::string loginA = "ws-disconnect-a-" + suffix;
    const std::string loginB = "ws-disconnect-b-" + suffix;
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
    auto clientB = std::make_unique<WsTestClient>(wsUrl);
    ASSERT_TRUE(clientA.waitConnected());
    ASSERT_TRUE(clientB->waitConnected());

    clientA.send(nlohmann::json{{"token", *tokenA}, {"channel_id", *channelId}});
    ASSERT_TRUE(clientA.waitFor([](const nlohmann::json& m) { return m.contains("subscribed"); }).has_value());
    clientB->send(nlohmann::json{{"token", *tokenB}, {"channel_id", *channelId}});
    ASSERT_TRUE(clientB->waitFor([](const nlohmann::json& m) { return m.contains("subscribed"); }).has_value());

    clientA.send(nlohmann::json{{"call_join", true}});
    ASSERT_TRUE(clientA.waitFor([](const nlohmann::json& m) { return m.contains("call_roster"); }).has_value());
    clientB->send(nlohmann::json{{"call_join", true}});
    ASSERT_TRUE(clientB->waitFor([](const nlohmann::json& m) { return m.contains("call_roster"); }).has_value());
    ASSERT_TRUE(clientA.waitFor([](const nlohmann::json& m) { return m.contains("call_peer_joined"); }).has_value());

    // B отключается, ни разу не отправив call_leave — A всё равно должен
    // получить уведомление через путь очистки Close/Error в handleMessage().
    clientB.reset();
    const std::optional<nlohmann::json> peerLeft =
        clientA.waitFor([](const nlohmann::json& m) { return m.contains("call_peer_left"); });
    ASSERT_TRUE(peerLeft.has_value());
    EXPECT_EQ((*peerLeft)["call_peer_left"].get<std::string>(), loginB);

    server.stop();
}

}  // namespace chat_service
