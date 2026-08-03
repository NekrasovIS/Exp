#include "auth/AuthClient.h"
#include "chat/ChatClient.h"

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

struct RestResult {
    bool ok = false;
    QJsonObject body;
};

RestResult postJson(QNetworkAccessManager& manager, const QUrl& url, const QJsonObject& body,
                     const QString& bearerToken = QString()) {
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    if (!bearerToken.isEmpty()) {
        request.setRawHeader("Authorization", "Bearer " + bearerToken.toUtf8());
    }

    QNetworkReply* reply = manager.post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));

    RestResult result;
    QEventLoop loop;
    QTimer::singleShot(3000, &loop, &QEventLoop::quit);
    QObject::connect(reply, &QNetworkReply::finished, &loop, [&]() {
        result.ok = (reply->error() == QNetworkReply::NoError);
        result.body = QJsonDocument::fromJson(reply->readAll()).object();
        reply->deleteLater();
        loop.quit();
    });
    loop.exec();
    return result;
}

TEST(ChatClientIntegrationTest, ConnectSendAndReceiveRoundTrip) {
    const QUrl authUrl(QString::fromStdString(envOrDefault("AUTH_SERVICE_URL", "http://127.0.0.1:8080")));
    const QUrl userUrl(QString::fromStdString(envOrDefault("USER_SERVICE_URL", "http://127.0.0.1:8081")));
    const QUrl chatRestUrl(QString::fromStdString(envOrDefault("CHAT_SERVICE_URL", "http://127.0.0.1:8082")));
    const QUrl chatWsUrl(QString::fromStdString(envOrDefault("CHAT_SERVICE_WS_URL", "ws://127.0.0.1:8083")));

    const QString login = QStringLiteral("chat-integration-test-%1").arg(QDateTime::currentMSecsSinceEpoch());
    const QString password = QStringLiteral("integration-test-password");

    QNetworkAccessManager manager;

    const RestResult registerResult =
        postJson(manager, userUrl.resolved(QUrl("/users/register")), {{"login", login}, {"password", password}});
    if (!registerResult.ok) {
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

    const RestResult communityResult =
        postJson(manager, chatRestUrl.resolved(QUrl("/communities")), {{"name", "integration-test"}}, token);
    ASSERT_TRUE(communityResult.ok);
    const auto communityId = communityResult.body.value("id").toVariant().toLongLong();

    const RestResult channelResult = postJson(
        manager, chatRestUrl.resolved(QUrl(QStringLiteral("/communities/%1/channels").arg(communityId))),
        {{"name", "general"}}, token);
    ASSERT_TRUE(channelResult.ok);
    const auto channelId = channelResult.body.value("id").toVariant().toLongLong();

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
