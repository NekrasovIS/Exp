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
    qint64 receivedId = 0;
    {
        QEventLoop loop;
        QTimer::singleShot(3000, &loop, &QEventLoop::quit);
        QObject::connect(&chatClient, &ChatClient::messageReceived, &loop,
                          [&](qint64 id, const QString& author, const QString& body, const QString&) {
                              receivedId = id;
                              receivedAuthor = author;
                              receivedBody = body;
                              loop.quit();
                          });
        chatClient.sendMessage(QStringLiteral("hello from ChatClientIntegrationTest"));
        loop.exec();
    }

    EXPECT_EQ(receivedAuthor, login);
    EXPECT_EQ(receivedBody, QStringLiteral("hello from ChatClientIntegrationTest"));
    ASSERT_GT(receivedId, 0);

    // issue #107: editing and deleting that same message round-trips
    // through the live server too.
    QString editedBody;
    QString editedAt;
    {
        QEventLoop loop;
        QTimer::singleShot(3000, &loop, &QEventLoop::quit);
        QObject::connect(&chatClient, &ChatClient::messageEdited, &loop,
                          [&](qint64, const QString& newBody, const QString& newEditedAt) {
                              editedBody = newBody;
                              editedAt = newEditedAt;
                              loop.quit();
                          });
        chatClient.sendEditMessage(receivedId, QStringLiteral("edited from ChatClientIntegrationTest"));
        loop.exec();
    }
    EXPECT_EQ(editedBody, QStringLiteral("edited from ChatClientIntegrationTest"));
    EXPECT_FALSE(editedAt.isEmpty());

    bool deleted = false;
    qint64 deletedId = 0;
    {
        QEventLoop loop;
        QTimer::singleShot(3000, &loop, &QEventLoop::quit);
        QObject::connect(&chatClient, &ChatClient::messageDeleted, &loop, [&](qint64 id) {
            deleted = true;
            deletedId = id;
            loop.quit();
        });
        chatClient.sendDeleteMessage(receivedId);
        loop.exec();
    }
    EXPECT_TRUE(deleted);
    EXPECT_EQ(deletedId, receivedId);
}

TEST(ChatClientIntegrationTest, DisconnectFromChannelStopsFurtherMessageDelivery) {
    const QUrl authUrl(QString::fromStdString(envOrDefault("AUTH_SERVICE_URL", "http://127.0.0.1:8080")));
    const QUrl userUrl(QString::fromStdString(envOrDefault("USER_SERVICE_URL", "http://127.0.0.1:8081")));
    const QUrl chatRestUrl(QString::fromStdString(envOrDefault("CHAT_SERVICE_URL", "http://127.0.0.1:8082")));
    const QUrl chatWsUrl(QString::fromStdString(envOrDefault("CHAT_SERVICE_WS_URL", "ws://127.0.0.1:8083")));

    const QString loginA = QStringLiteral("chat-disconnect-test-a-%1").arg(QDateTime::currentMSecsSinceEpoch());
    const QString loginB = QStringLiteral("chat-disconnect-test-b-%1").arg(QDateTime::currentMSecsSinceEpoch());
    const QString password = QStringLiteral("integration-test-password");

    QNetworkAccessManager manager;
    if (!registerTestUser(manager, userUrl, loginA, password) || !registerTestUser(manager, userUrl, loginB, password)) {
        GTEST_SKIP() << "user-service not reachable — start the full stack to run this test.";
    }

    AuthClient authClientA(authUrl);
    QString tokenA;
    {
        QEventLoop loop;
        QTimer::singleShot(3000, &loop, &QEventLoop::quit);
        QObject::connect(&authClientA, &AuthClient::tokenReceived, &loop, [&](const QString& token) {
            tokenA = token;
            loop.quit();
        });
        QObject::connect(&authClientA, &AuthClient::errorOccurred, &loop, [&](const QString&) { loop.quit(); });
        authClientA.requestToken(loginA, password);
        loop.exec();
    }
    AuthClient authClientB(authUrl);
    QString tokenB;
    {
        QEventLoop loop;
        QTimer::singleShot(3000, &loop, &QEventLoop::quit);
        QObject::connect(&authClientB, &AuthClient::tokenReceived, &loop, [&](const QString& token) {
            tokenB = token;
            loop.quit();
        });
        authClientB.requestToken(loginB, password);
        loop.exec();
    }
    if (tokenA.isEmpty() || tokenB.isEmpty()) {
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
        chatRestClient.createCommunity(tokenA, QStringLiteral("disconnect-test"));
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
        chatRestClient.createChannel(tokenA, communityId, QStringLiteral("general"));
        loop.exec();
    }
    ASSERT_GT(channelId, 0);

    ChatClient chatClientA(chatWsUrl);
    {
        QEventLoop loop;
        QTimer::singleShot(3000, &loop, &QEventLoop::quit);
        QObject::connect(&chatClientA, &ChatClient::subscribed, &loop, [&](qint64) { loop.quit(); });
        chatClientA.connectToChannel(tokenA, channelId);
        loop.exec();
    }

    int messagesReceivedByA = 0;
    QObject::connect(&chatClientA, &ChatClient::messageReceived, &chatClientA,
                      [&](qint64, const QString&, const QString&, const QString&) { ++messagesReceivedByA; });

    chatClientA.disconnectFromChannel();

    // Give the disconnect a moment to actually take effect before B posts.
    {
        QEventLoop loop;
        QTimer::singleShot(300, &loop, &QEventLoop::quit);
        loop.exec();
    }

    ChatClient chatClientB(chatWsUrl);
    {
        QEventLoop loop;
        QTimer::singleShot(3000, &loop, &QEventLoop::quit);
        QObject::connect(&chatClientB, &ChatClient::subscribed, &loop, [&](qint64) { loop.quit(); });
        chatClientB.connectToChannel(tokenB, channelId);
        loop.exec();
    }
    {
        QEventLoop loop;
        QTimer::singleShot(3000, &loop, &QEventLoop::quit);
        QObject::connect(&chatClientB, &ChatClient::messageReceived, &loop,
                          [&](qint64, const QString&, const QString&, const QString&) { loop.quit(); });
        chatClientB.sendMessage(QStringLiteral("posted after A disconnected"));
        loop.exec();
    }

    EXPECT_EQ(messagesReceivedByA, 0);
}

TEST(ChatClientIntegrationTest, CallSignalingRoundTripBetweenTwoClientsDirectly) {
    // Exercises joinCall/leaveCall/sendCallSignal and their matching
    // signals directly on ChatClient — decoupled from CallManager/WebRTC
    // and real audio/camera hardware, unlike CallManagerIntegrationTest.
    const QUrl authUrl(QString::fromStdString(envOrDefault("AUTH_SERVICE_URL", "http://127.0.0.1:8080")));
    const QUrl userUrl(QString::fromStdString(envOrDefault("USER_SERVICE_URL", "http://127.0.0.1:8081")));
    const QUrl chatRestUrl(QString::fromStdString(envOrDefault("CHAT_SERVICE_URL", "http://127.0.0.1:8082")));
    const QUrl chatWsUrl(QString::fromStdString(envOrDefault("CHAT_SERVICE_WS_URL", "ws://127.0.0.1:8083")));

    const QString loginA = QStringLiteral("chat-callsig-test-a-%1").arg(QDateTime::currentMSecsSinceEpoch());
    const QString loginB = QStringLiteral("chat-callsig-test-b-%1").arg(QDateTime::currentMSecsSinceEpoch());
    const QString password = QStringLiteral("integration-test-password");

    QNetworkAccessManager manager;
    if (!registerTestUser(manager, userUrl, loginA, password) || !registerTestUser(manager, userUrl, loginB, password)) {
        GTEST_SKIP() << "user-service not reachable — start the full stack to run this test.";
    }

    AuthClient authClientA(authUrl);
    QString tokenA;
    {
        QEventLoop loop;
        QTimer::singleShot(3000, &loop, &QEventLoop::quit);
        QObject::connect(&authClientA, &AuthClient::tokenReceived, &loop, [&](const QString& token) {
            tokenA = token;
            loop.quit();
        });
        authClientA.requestToken(loginA, password);
        loop.exec();
    }
    AuthClient authClientB(authUrl);
    QString tokenB;
    {
        QEventLoop loop;
        QTimer::singleShot(3000, &loop, &QEventLoop::quit);
        QObject::connect(&authClientB, &AuthClient::tokenReceived, &loop, [&](const QString& token) {
            tokenB = token;
            loop.quit();
        });
        authClientB.requestToken(loginB, password);
        loop.exec();
    }
    if (tokenA.isEmpty() || tokenB.isEmpty()) {
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
        chatRestClient.createCommunity(tokenA, QStringLiteral("callsig-test"));
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
        chatRestClient.createChannel(tokenA, communityId, QStringLiteral("general"));
        loop.exec();
    }
    ASSERT_GT(channelId, 0);

    ChatClient chatClientA(chatWsUrl);
    ChatClient chatClientB(chatWsUrl);
    {
        QEventLoop loop;
        QTimer::singleShot(3000, &loop, &QEventLoop::quit);
        QObject::connect(&chatClientA, &ChatClient::subscribed, &loop, [&](qint64) { loop.quit(); });
        chatClientA.connectToChannel(tokenA, channelId);
        loop.exec();
    }
    {
        QEventLoop loop;
        QTimer::singleShot(3000, &loop, &QEventLoop::quit);
        QObject::connect(&chatClientB, &ChatClient::subscribed, &loop, [&](qint64) { loop.quit(); });
        chatClientB.connectToChannel(tokenB, channelId);
        loop.exec();
    }

    // A joins the call first — empty roster.
    QStringList rosterForA;
    bool rosterForAFired = false;
    {
        QEventLoop loop;
        QTimer::singleShot(3000, &loop, &QEventLoop::quit);
        QObject::connect(&chatClientA, &ChatClient::callRosterReceived, &loop, [&](const QStringList& participants) {
            rosterForA = participants;
            rosterForAFired = true;
            loop.quit();
        });
        chatClientA.joinCall();
        loop.exec();
    }
    ASSERT_TRUE(rosterForAFired);
    EXPECT_TRUE(rosterForA.isEmpty());

    // B joins second — sees A, and A is notified of B.
    QString peerJoinedOnA;
    QObject::connect(&chatClientA, &ChatClient::callPeerJoined, &chatClientA,
                      [&](const QString& login) { peerJoinedOnA = login; });
    QStringList rosterForB;
    {
        QEventLoop loop;
        QTimer::singleShot(3000, &loop, &QEventLoop::quit);
        QObject::connect(&chatClientB, &ChatClient::callRosterReceived, &loop, [&](const QStringList& participants) {
            rosterForB = participants;
            loop.quit();
        });
        chatClientB.joinCall();
        loop.exec();
    }
    ASSERT_EQ(rosterForB.size(), 1);
    EXPECT_EQ(rosterForB[0], loginA);

    // Give A's queued callPeerJoined a moment to arrive.
    {
        QEventLoop loop;
        QTimer::singleShot(300, &loop, &QEventLoop::quit);
        loop.exec();
    }
    EXPECT_EQ(peerJoinedOnA, loginB);

    // B signals A directly — A receives it via callSignalReceived.
    QString signalFrom;
    QJsonObject signalPayload;
    {
        QEventLoop loop;
        QTimer::singleShot(3000, &loop, &QEventLoop::quit);
        QObject::connect(&chatClientA, &ChatClient::callSignalReceived, &loop,
                          [&](const QString& from, const QJsonObject& payload) {
                              signalFrom = from;
                              signalPayload = payload;
                              loop.quit();
                          });
        chatClientB.sendCallSignal(loginA, QJsonObject{{"kind", "offer"}, {"sdp", "fake-sdp-direct-test"}});
        loop.exec();
    }
    EXPECT_EQ(signalFrom, loginB);
    EXPECT_EQ(signalPayload.value("sdp").toString(), QStringLiteral("fake-sdp-direct-test"));

    // B leaves — A is notified.
    QString peerLeftOnA;
    {
        QEventLoop loop;
        QTimer::singleShot(3000, &loop, &QEventLoop::quit);
        QObject::connect(&chatClientA, &ChatClient::callPeerLeft, &loop, [&](const QString& login) {
            peerLeftOnA = login;
            loop.quit();
        });
        chatClientB.leaveCall();
        loop.exec();
    }
    EXPECT_EQ(peerLeftOnA, loginB);
}

}  // namespace
}  // namespace devicehub
