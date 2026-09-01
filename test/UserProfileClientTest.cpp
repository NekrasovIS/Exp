#include "auth/AuthClient.h"
#include "user/UserProfileClient.h"

#include <gtest/gtest.h>

#include <QDateTime>
#include <QEventLoop>
#include <QTimer>
#include <QUrl>

#include <cstdlib>
#include <utility>

// Requires a live auth-service + user-service (issue #110's profile
// routes live on user-service). Skips itself rather than failing when
// the stack isn't running — same pattern as ChatRestClientTest.cpp.

namespace devicehub {
namespace {

std::string envOrDefault(const char* name, const std::string& defaultValue) {
    const char* value = std::getenv(name);
    return value != nullptr ? std::string(value) : defaultValue;
}

QUrl authServiceUrl() {
    return QUrl(QString::fromStdString(envOrDefault("AUTH_SERVICE_URL", "http://127.0.0.1:8080")));
}

QUrl userServiceUrl() {
    return QUrl(QString::fromStdString(envOrDefault("USER_SERVICE_URL", "http://127.0.0.1:8081")));
}

QString uniqueLogin(const QString& prefix) {
    return prefix + QStringLiteral("-") + QString::number(QDateTime::currentMSecsSinceEpoch());
}

// Registers a brand-new user against a live auth-service and returns
// {token, login}, or two empty strings if the stack isn't reachable
// within the timeout.
std::pair<QString, QString> registerAndGetTokenAndLogin(const QString& loginPrefix) {
    AuthClient authClient(authServiceUrl());
    const QString login = uniqueLogin(loginPrefix);
    QString token;
    QEventLoop loop;
    QTimer::singleShot(3000, &loop, &QEventLoop::quit);
    QObject::connect(&authClient, &AuthClient::tokenReceived, &loop, [&](const QString& receivedToken) {
        token = receivedToken;
        loop.quit();
    });
    QObject::connect(&authClient, &AuthClient::errorOccurred, &loop, [&](const QString&) { loop.quit(); });
    authClient.registerUser(login, QStringLiteral("user-profile-client-test-password"));
    loop.exec();
    return {token, login};
}

TEST(UserProfileClientTest, UpdateOwnProfileRoundTripsThroughFetchProfile) {
    const auto [token, login] = registerAndGetTokenAndLogin(QStringLiteral("user-profile-client-update"));
    if (token.isEmpty()) {
        GTEST_SKIP() << "auth-service/user-service not reachable — start the stack to run this test.";
    }

    // Unique per run, same as login — a hardcoded email literal would
    // collide with a previous run's row on user-service's partial
    // unique index (issue #156) the moment this binary runs a second
    // time against the same database, turning updateOwnProfile() into
    // an unrelated 409 that this test (only listening for
    // profileUpdated(), not errorOccurred()) would just silently time
    // out on instead of failing with a clear message.
    const QString email = login + QStringLiteral("@example.test");

    UserProfileClient client(userServiceUrl());
    UserProfile updated;
    QString updateError;
    {
        QEventLoop loop;
        QTimer::singleShot(3000, &loop, &QEventLoop::quit);
        QObject::connect(&client, &UserProfileClient::profileUpdated, &loop, [&](const UserProfile& profile) {
            updated = profile;
            loop.quit();
        });
        QObject::connect(&client, &UserProfileClient::errorOccurred, &loop, [&](const QString& message) {
            updateError = message;
            loop.quit();
        });
        client.updateOwnProfile(
            token, ProfileEdits{.displayName = QStringLiteral("Alice"),
                                 .avatarUrl = QStringLiteral("https://example.test/alice.png"), .email = email});
        loop.exec();
    }
    ASSERT_TRUE(updateError.isEmpty()) << updateError.toStdString();
    EXPECT_EQ(updated.login, login);
    EXPECT_EQ(updated.displayName, QStringLiteral("Alice"));
    EXPECT_EQ(updated.avatarUrl, QStringLiteral("https://example.test/alice.png"));
    EXPECT_EQ(updated.email, email);

    UserProfile fetched;
    {
        QEventLoop loop;
        QTimer::singleShot(3000, &loop, &QEventLoop::quit);
        QObject::connect(&client, &UserProfileClient::profileReceived, &loop, [&](const UserProfile& profile) {
            fetched = profile;
            loop.quit();
        });
        client.fetchProfile(token, login);
        loop.exec();
    }
    EXPECT_EQ(fetched.displayName, QStringLiteral("Alice"));
    EXPECT_EQ(fetched.avatarUrl, QStringLiteral("https://example.test/alice.png"));
    EXPECT_EQ(fetched.email, email);
}

TEST(UserProfileClientTest, PublishPublicKeyRoundTripsThroughFetchProfile) {
    // issue #136 (E2E encryption Phase 1): publishing a key is a
    // separate call from updateOwnProfile() — this confirms it goes to
    // the same underlying profile without disturbing display_name/
    // avatar_url (fetch-then-merge on the server side).
    const auto [token, login] = registerAndGetTokenAndLogin(QStringLiteral("user-profile-client-pubkey"));
    if (token.isEmpty()) {
        GTEST_SKIP() << "auth-service/user-service not reachable — start the stack to run this test.";
    }

    UserProfileClient client(userServiceUrl());
    UserProfile updated;
    {
        QEventLoop loop;
        QTimer::singleShot(3000, &loop, &QEventLoop::quit);
        QObject::connect(&client, &UserProfileClient::profileUpdated, &loop, [&](const UserProfile& profile) {
            updated = profile;
            loop.quit();
        });
        client.publishPublicKey(token, QStringLiteral("base64-x25519-public-key"));
        loop.exec();
    }
    EXPECT_EQ(updated.login, login);
    EXPECT_EQ(updated.publicKey, QStringLiteral("base64-x25519-public-key"));

    UserProfile fetched;
    {
        QEventLoop loop;
        QTimer::singleShot(3000, &loop, &QEventLoop::quit);
        QObject::connect(&client, &UserProfileClient::profileReceived, &loop, [&](const UserProfile& profile) {
            fetched = profile;
            loop.quit();
        });
        client.fetchProfile(token, login);
        loop.exec();
    }
    EXPECT_EQ(fetched.publicKey, QStringLiteral("base64-x25519-public-key"));
}

TEST(UserProfileClientTest, FetchProfileWithoutAuthorizationEmitsError) {
    const auto [token, login] = registerAndGetTokenAndLogin(QStringLiteral("user-profile-client-noauth"));
    if (token.isEmpty()) {
        GTEST_SKIP() << "auth-service/user-service not reachable — start the stack to run this test.";
    }

    UserProfileClient client(userServiceUrl());
    QString errorMessage;
    QEventLoop loop;
    QTimer::singleShot(3000, &loop, &QEventLoop::quit);
    QObject::connect(&client, &UserProfileClient::errorOccurred, &loop, [&](const QString& message) {
        errorMessage = message;
        loop.quit();
    });
    client.fetchProfile(QStringLiteral("not-a-real-token"), login);
    loop.exec();

    EXPECT_FALSE(errorMessage.isEmpty());
}

}  // namespace
}  // namespace devicehub
