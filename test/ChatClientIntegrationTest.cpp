#include "auth/AuthClient.h"
#include "chat/ChatClient.h"
#include "chat/ChatRestClient.h"

#include <gtest/gtest.h>

#include <QDateTime>
#include <QEventLoop>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>

#include <cstdlib>

// Requires the full live stack: auth-service, user-service, chat-service
// (REST + WebSocket), and both Postgres instances (see docker-compose.yml).
// Skips itself rather than failing when it isn't running — CI doesn't
// currently orchestrate all of this together, so this is a manual/local
// end-to-end check.

namespace devicehub {
namespace {

std::string envOrDefault(const char* name, const std::string& defaultValue) {
    const char* value = std::getenv(name);
    return value != nullptr ? std::string(value) : defaultValue;
}

// user-service registration isn't part of ChatRestClient's scope (that
// client only talks to chat-service), so this stays a one-off helper.
bool registerTestUser(QNetworkAccessManager& manager, const QUrl& userServiceUrl, const QString& login,
                       const QString& password) {
    QNetworkRequest request(userServiceUrl.resolved(QUrl(QStringLiteral("/users/register"))));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));

    QNetworkReply* reply =
        manager.post(request, QJsonDocument(QJsonObject{{"login", login}, {"password", password}}).toJson());

    bool succeeded = false;
    QEventLoop loop;
    QTimer::singleShot(3000, &loop, &QEventLoop::quit);
    QObject::connect(reply, &QNetworkReply::finished, &loop, [&]() {
        succeeded = (reply->error() == QNetworkReply::NoError);
        reply->deleteLater();
        loop.quit();
    });
    loop.exec();
    return succeeded;
}

TEST(ChatClientIntegrationTest, ConnectSendAndReceiveRoundTrip) {
    const QUrl authUrl(QString::fromStdString(envOrDefault("AUTH_SERVICE_URL", "http://127.0.0.1:8080")));
    const QUrl userUrl(QString::fromStdString(envOrDefault("USER_SERVICE_URL", "http://127.0.0.1:8081")));
    const QUrl chatRestUrl(QString::fromStdString(envOrDefault("CHAT_SERVICE_URL", "http://127.0.0.1:8082")));
    const QUrl chatWsUrl(QString::fromStdString(envOrDefault("CHAT_SERVICE_WS_URL", "ws://127.0.0.1:8083")));

    const QString login = QStringLiteral("chat-integration-test-%1").arg(QDateTime::currentMSecsSinceEpoch());
    const QString password = QStringLiteral("integration-test-password");

    QNetworkAccessManager manager;
    if (!registerTestUser(manager, userUrl, login, password)) {
        GTEST_SKIP() << "user-service not reachable — start the full stack (docker compose + all three "
                        "services) to run this test.";
    }

    AuthClient authClient(authUrl);
    QString token;
    {
        QEventLoop loop;
        QTimer::singleShot(3000, &loop, &QEventLoop::quit);
        QObject::connect(&authClient, &AuthClient::tokenReceived, &loop, [&](const QString& receivedToken) {
            token = receivedToken;
            loop.quit();
        });
        QObject::connect(&authClient, &AuthClient::errorOccurred, &loop, [&](const QString&) { loop.quit(); });
        authClient.requestToken(login, password);
        loop.exec();
    }
    if (token.isEmpty()) {
        GTEST_SKIP() << "auth-service not reachable.";
    }

    ChatRestClient chatRestClient(chatRestUrl);

    qint64 communityId = 0;
    {
        QEventLoop loop;
        QTimer::singleShot(3000, &loop, &QEventLoop::quit);
        QObject::connect(&chatRestClient, &ChatRestClient::communityCreated, &loop, [&](qint64 id, const QString&) {
            communityId = id;
            loop.quit();
        });
        chatRestClient.createCommunity(token, QStringLiteral("integration-test"));
        loop.exec();
    }
    ASSERT_GT(communityId, 0);

    qint64 channelId = 0;
    {
        QEventLoop loop;
        QTimer::singleShot(3000, &loop, &QEventLoop::quit);
        QObject::connect(&chatRestClient, &ChatRestClient::channelCreated, &loop, [&](qint64 id, const QString&) {
            channelId = id;
            loop.quit();
        });
        chatRestClient.createChannel(token, communityId, QStringLiteral("general"));
        loop.exec();
    }
    ASSERT_GT(channelId, 0);

    ChatClient chatClient(chatWsUrl);
    bool subscribed = false;
    {
        QEventLoop loop;
        QTimer::singleShot(3000, &loop, &QEventLoop::quit);
        QObject::connect(&chatClient, &ChatClient::subscribed, &loop, [&](qint64) {
            subscribed = true;
            loop.quit();
        });
        chatClient.connectToChannel(token, channelId);
        loop.exec();
    }
    ASSERT_TRUE(subscribed);

    QString receivedAuthor;
    QString receivedBody;
    {
        QEventLoop loop;
        QTimer::singleShot(3000, &loop, &QEventLoop::quit);
        QObject::connect(&chatClient, &ChatClient::messageReceived, &loop,
                          [&](const QString& author, const QString& body, const QString&) {
                              receivedAuthor = author;
                              receivedBody = body;
                              loop.quit();
                          });
        chatClient.sendMessage(QStringLiteral("hello from ChatClientIntegrationTest"));
        loop.exec();
    }

    EXPECT_EQ(receivedAuthor, login);
    EXPECT_EQ(receivedBody, QStringLiteral("hello from ChatClientIntegrationTest"));
}

}  // namespace
}  // namespace devicehub
