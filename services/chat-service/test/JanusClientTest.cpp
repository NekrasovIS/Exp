#include "JanusClient.h"

#include <gtest/gtest.h>
#include <httplib.h>

#include <chrono>
#include <cstdlib>
#include <string>

// Требует работающий Janus, доступный по JANUS_HOST/PORT (по умолчанию
// 127.0.0.1:8088, docker-compose.yml профиль "sfu") — пропускает себя, а
// не падает, если он не запущен. Случай недоступного Janus не требует
// работающего сервиса и выполняется всегда.

namespace chat_service {
namespace {

std::string envOrDefault(const char* name, const std::string& defaultValue) {
    const char* value = std::getenv(name);
    return value != nullptr ? std::string(value) : defaultValue;
}

bool janusReachable(const std::string& host, int port) {
    httplib::Client client(host, port);
    return static_cast<bool>(client.Post("/janus", R"({"janus":"create","transaction":"probe"})", "application/json"));
}

std::string uniqueRoomId() {
    return "janus-client-test-" +
           std::to_string(
               std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
                   .count());
}

TEST(JanusClientTest, EnsureRoomExistsCreatesRoomOnFirstCall) {
    const std::string host = envOrDefault("JANUS_HOST", "127.0.0.1");
    const int port = std::stoi(envOrDefault("JANUS_PORT", "8088"));
    if (!janusReachable(host, port)) {
        GTEST_SKIP() << "Janus not reachable at " << host << ":" << port
                      << " — run `docker compose --profile sfu up -d janus` to run this test.";
    }

    const JanusClient client(host, port);
    EXPECT_TRUE(client.ensureRoomExists(uniqueRoomId()));
}

TEST(JanusClientTest, EnsureRoomExistsIsIdempotentForAnAlreadyExistingRoom) {
    const std::string host = envOrDefault("JANUS_HOST", "127.0.0.1");
    const int port = std::stoi(envOrDefault("JANUS_PORT", "8088"));
    if (!janusReachable(host, port)) {
        GTEST_SKIP() << "Janus not reachable at " << host << ":" << port
                      << " — run `docker compose --profile sfu up -d janus` to run this test.";
    }

    const JanusClient client(host, port);
    const std::string roomId = uniqueRoomId();

    ASSERT_TRUE(client.ensureRoomExists(roomId));
    // Второй вызов не должен упасть/провалиться из-за того, что комната
    // уже существует (error_code 427 из Janus обрабатывается как успех) —
    // это и есть идемпотентность, которую WebSocketServer::handleCallJoin()
    // требует от повторного call_join.
    EXPECT_TRUE(client.ensureRoomExists(roomId));
}

TEST(JanusClientTest, EnsureRoomExistsFailsClosedWhenJanusIsUnreachable) {
    // Логики пропуска намеренно нет — цель тут неиспользуемый loopback-порт.
    const JanusClient client("127.0.0.1", 1);

    EXPECT_FALSE(client.ensureRoomExists("any-room"));
}

}  // namespace
}  // namespace chat_service
