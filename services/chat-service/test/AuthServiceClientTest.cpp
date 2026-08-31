#include "AuthServiceClient.h"

#include <gtest/gtest.h>
#include <httplib.h>

#include <chrono>
#include <cstdlib>
#include <nlohmann/json.hpp>
#include <string>

// Requires a live auth-service reachable at AUTH_SERVICE_HOST/PORT
// (defaults 127.0.0.1:8080), same as WebSocketServerTest.cpp — skips
// itself rather than failing when it isn't running. The unreachable-
// service case needs no live service and always runs.

namespace chat_service {
namespace {

std::string envOrDefault(const char* name, const std::string& defaultValue) {
    const char* value = std::getenv(name);
    return value != nullptr ? std::string(value) : defaultValue;
}

bool authServiceReachable(const std::string& host, int port) {
    httplib::Client client(host, port);
    const nlohmann::json body{{"token", "reachability-probe"}};
    return static_cast<bool>(client.Post("/auth/verify", body.dump(), "application/json"));
}

TEST(AuthServiceClientTest, VerifyTokenReturnsSubjectForValidToken) {
    const std::string host = envOrDefault("AUTH_SERVICE_HOST", "127.0.0.1");
    const int port = std::stoi(envOrDefault("AUTH_SERVICE_PORT", "8080"));
    if (!authServiceReachable(host, port)) {
        GTEST_SKIP() << "auth-service not reachable at " << host << ":" << port
                      << " — start it locally to run this test.";
    }

    const std::string login =
        "auth-service-client-test-" +
        std::to_string(
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
                .count());
    httplib::Client registerClient(host, port);
    const nlohmann::json registerBody{{"login", login}, {"password", "auth-service-client-test-password"}};
    const httplib::Result registerResult = registerClient.Post("/auth/register", registerBody.dump(), "application/json");
    ASSERT_TRUE(registerResult);
    ASSERT_EQ(registerResult->status, 201);
    const nlohmann::json registerResponse = nlohmann::json::parse(registerResult->body);
    const std::string token = registerResponse["token"].get<std::string>();

    const AuthServiceClient client(host, port);
    const std::optional<std::string> subject = client.verifyToken(token);

    ASSERT_TRUE(subject.has_value());
    EXPECT_EQ(*subject, login);
}

TEST(AuthServiceClientTest, VerifyTokenReturnsNulloptForInvalidToken) {
    const std::string host = envOrDefault("AUTH_SERVICE_HOST", "127.0.0.1");
    const int port = std::stoi(envOrDefault("AUTH_SERVICE_PORT", "8080"));
    if (!authServiceReachable(host, port)) {
        GTEST_SKIP() << "auth-service not reachable at " << host << ":" << port
                      << " — start it locally to run this test.";
    }

    const AuthServiceClient client(host, port);

    EXPECT_FALSE(client.verifyToken("not-a-real-token").has_value());
}

TEST(AuthServiceClientTest, VerifyTokenFailsClosedWhenAuthServiceIsUnreachable) {
    // No skip logic on purpose — targets an unused loopback port.
    const AuthServiceClient client("127.0.0.1", 1);

    EXPECT_FALSE(client.verifyToken("any-token").has_value());
}

}  // namespace
}  // namespace chat_service
