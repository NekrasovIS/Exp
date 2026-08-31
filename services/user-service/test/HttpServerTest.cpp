#include "HttpServer.h"

#include <gtest/gtest.h>
#include <httplib.h>

#include <chrono>
#include <cstdlib>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <thread>

#include "UserRepository.h"
#include "UserService.h"

// Route-level tests for HttpServer, hitting a real httplib::Server
// instance over loopback HTTP. Requires a live Postgres (see
// docker-compose.yml) reachable at USER_SERVICE_DATABASE_URL, same as
// UserServiceIntegrationTest/UserRepositoryTest — skips itself rather
// than fail when it isn't running.

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

// Starts a real HttpServer on a background thread for the lifetime of
// the fixture and stops it on destruction.
class ScopedServer {
public:
    explicit ScopedServer(UserService& userService) : server_(userService), thread_([this] { server_.listen(kTestHost, kTestPort); }) {
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
    const ScopedServer server(userService);

    httplib::Client client(kTestHost, kTestPort);
    const httplib::Result result =
        client.Post("/users/register", nlohmann::json{{"login", "alice"}}.dump(), "application/json");

    ASSERT_TRUE(result);
    EXPECT_EQ(result->status, 400);
}

TEST(HttpServerTest, RegisterRouteRejectsMalformedJsonWith400) {
    UserRepository repository(connectionString());
    UserService userService(repository);
    const ScopedServer server(userService);

    httplib::Client client(kTestHost, kTestPort);
    const httplib::Result result = client.Post("/users/register", "not json", "application/json");

    ASSERT_TRUE(result);
    EXPECT_EQ(result->status, 400);
}

TEST(HttpServerTest, RegisterRouteCreatesAccountWith201) {
    UserRepository repository(connectionString());
    UserService userService(repository);

    const std::string login = uniqueLogin("http-server-register-test");
    // Reachability probe: registering the throwaway login below will
    // throw (not just fail) if Postgres isn't up, since UserRepository
    // opens a real connection per call.
    try {
        static_cast<void>(userService.registerUser("http-server-test-reachability-probe", "irrelevant"));
    } catch (const std::exception& error) {
        GTEST_SKIP() << "Postgres not reachable (" << error.what() << ") — run `docker compose up` to run this test.";
    }

    const ScopedServer server(userService);
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

    const ScopedServer server(userService);
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
    const ScopedServer server(userService);

    httplib::Client client(kTestHost, kTestPort);
    const httplib::Result result =
        client.Post("/users/verify-credentials", nlohmann::json{{"login", "alice"}}.dump(), "application/json");

    ASSERT_TRUE(result);
    EXPECT_EQ(result->status, 400);
}

TEST(HttpServerTest, VerifyCredentialsRouteRejectsMalformedJsonWith400) {
    UserRepository repository(connectionString());
    UserService userService(repository);
    const ScopedServer server(userService);

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

    const ScopedServer server(userService);
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

    const ScopedServer server(userService);
    httplib::Client client(kTestHost, kTestPort);
    const httplib::Result result = client.Post(
        "/users/verify-credentials", nlohmann::json{{"login", login}, {"password", "wrong-password"}}.dump(),
        "application/json");

    ASSERT_TRUE(result);
    ASSERT_EQ(result->status, 200);
    const nlohmann::json body = nlohmann::json::parse(result->body);
    EXPECT_FALSE(body["valid"].get<bool>());
}

}  // namespace
}  // namespace user_service
