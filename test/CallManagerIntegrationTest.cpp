#include "auth/AuthClient.h"
#include "chat/CallManager.h"
#include "chat/ChatClient.h"
#include "chat/ChatRestClient.h"
#include "devices/AudioInputDevice.h"
#include "devices/AudioOutputDevice.h"
#include "devices/CameraDevice.h"
#include "devices/DeviceEnumerator.h"

#include <gtest/gtest.h>

#include <QAudioDevice>
#include <QCameraDevice>
#include <QDateTime>
#include <QEventLoop>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>

#include <cstdlib>

// Requires the full live stack: auth-service, user-service, chat-service
// (REST + WebSocket), and both Postgres instances (see docker-compose.yml),
// plus real network access to gather ICE candidates. Skips itself rather
// than failing when the stack isn't running — same convention as
// ChatClientIntegrationTest.
//
// This exercises the real offer/answer/signaling round trip (SDP
// generation, relay through live chat-service, parsing, auto-answer) via
// CallManager::callError — it does NOT wait for full ICE/DTLS
// connectivity, since that depends on STUN reachability and timing that
// would make the test flaky in a sandboxed CI environment; the signaling
// correctness it does verify is the part actually implemented in this
// change.

namespace devicehub {
namespace {

std::string envOrDefault(const char* name, const std::string& defaultValue) {
    const char* value = std::getenv(name);
    return value != nullptr ? std::string(value) : defaultValue;
}

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

std::optional<QString> requestToken(AuthClient& authClient, const QString& login, const QString& password) {
    QString token;
    QEventLoop loop;
    QTimer::singleShot(3000, &loop, &QEventLoop::quit);
    QObject::connect(&authClient, &AuthClient::tokenReceived, &loop, [&](const QString& receivedToken) {
        token = receivedToken;
        loop.quit();
    });
    QObject::connect(&authClient, &AuthClient::errorOccurred, &loop, [&](const QString&) { loop.quit(); });
    authClient.requestToken(login, password);
    loop.exec();
    return token.isEmpty() ? std::nullopt : std::make_optional(token);
}

}  // namespace

TEST(CallManagerIntegrationTest, OfferAnswerSignalingRoundTripWithoutErrors) {
    const QUrl authUrl(QString::fromStdString(envOrDefault("AUTH_SERVICE_URL", "http://127.0.0.1:8080")));
    const QUrl userUrl(QString::fromStdString(envOrDefault("USER_SERVICE_URL", "http://127.0.0.1:8081")));
    const QUrl chatRestUrl(QString::fromStdString(envOrDefault("CHAT_SERVICE_URL", "http://127.0.0.1:8082")));
    const QUrl chatWsUrl(QString::fromStdString(envOrDefault("CHAT_SERVICE_WS_URL", "ws://127.0.0.1:8083")));

    const qint64 suffix = QDateTime::currentMSecsSinceEpoch();
    const QString loginA = QStringLiteral("call-test-a-%1").arg(suffix);
    const QString loginB = QStringLiteral("call-test-b-%1").arg(suffix);
    const QString password = QStringLiteral("integration-test-password");

    QNetworkAccessManager manager;
    if (!registerTestUser(manager, userUrl, loginA, password) ||
        !registerTestUser(manager, userUrl, loginB, password)) {
        GTEST_SKIP() << "user-service not reachable — start the full stack to run this test.";
    }

    AuthClient authClientA(authUrl);
    AuthClient authClientB(authUrl);
    const std::optional<QString> tokenA = requestToken(authClientA, loginA, password);
    const std::optional<QString> tokenB = requestToken(authClientB, loginB, password);
    if (!tokenA.has_value() || !tokenB.has_value()) {
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
        chatRestClient.createCommunity(*tokenA, QStringLiteral("call-integration-test"));
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
        chatRestClient.createChannel(*tokenA, communityId, QStringLiteral("general"));
        loop.exec();
    }
    ASSERT_GT(channelId, 0);

    ChatClient chatClientA(chatWsUrl);
    ChatClient chatClientB(chatWsUrl);
    for (auto* client : {&chatClientA, &chatClientB}) {
        bool subscribed = false;
        QEventLoop loop;
        QTimer::singleShot(3000, &loop, &QEventLoop::quit);
        QObject::connect(client, &ChatClient::subscribed, &loop, [&](qint64) {
            subscribed = true;
            loop.quit();
        });
        client->connectToChannel(client == &chatClientA ? *tokenA : *tokenB, channelId);
        loop.exec();
        ASSERT_TRUE(subscribed);
    }

    // Real enumerated devices, not a default-constructed QAudioDevice —
    // CallManager::joinCall() treats a null device as "none selected"
    // and skips it with a callError() rather than crashing, which would
    // make this test pass for the wrong reason (never actually
    // exercising the real device wiring this test is meant to cover).
    DeviceEnumerator enumerator;
    const QList<QAudioDevice> outputs = enumerator.audioOutputs();
    const QList<QAudioDevice> inputs = enumerator.audioInputs();
    if (outputs.isEmpty() || inputs.isEmpty()) {
        GTEST_SKIP() << "No audio input/output device available on this machine.";
    }
    const QAudioDevice outputDevice = outputs.first();
    const QAudioDevice inputDevice = inputs.first();

    AudioInputDevice audioInputA;
    AudioOutputDevice audioOutputA;
    AudioInputDevice audioInputB;
    AudioOutputDevice audioOutputB;
    CameraDevice cameraA;
    CameraDevice cameraB;

    CallManager callManagerA(chatClientA, audioInputA, audioOutputA, cameraA);
    CallManager callManagerB(chatClientB, audioInputB, audioOutputB, cameraB);

    QStringList errorsA;
    QStringList errorsB;
    QObject::connect(&callManagerA, &CallManager::callError, [&](const QString& message) { errorsA << message; });
    QObject::connect(&callManagerB, &CallManager::callError, [&](const QString& message) { errorsB << message; });

    bool aSawBJoin = false;
    QObject::connect(&callManagerA, &CallManager::participantJoined, [&](const QString& login) {
        if (login == loginB) {
            aSawBJoin = true;
        }
    });

    callManagerA.joinCall(inputDevice, outputDevice);

    {
        // A joined alone (empty roster) — nothing to negotiate yet, just
        // give the join frame time to round-trip before B joins.
        QEventLoop loop;
        QTimer::singleShot(500, &loop, &QEventLoop::quit);
        loop.exec();
    }

    callManagerB.joinCall(inputDevice, outputDevice);

    {
        // Wait for the offer/answer/signaling exchange (relayed through
        // live chat-service) to settle.
        QEventLoop loop;
        QTimer::singleShot(4000, &loop, &QEventLoop::quit);
        loop.exec();
    }

    EXPECT_TRUE(aSawBJoin);
    EXPECT_TRUE(errorsA.isEmpty()) << errorsA.join(QStringLiteral("; ")).toStdString();
    EXPECT_TRUE(errorsB.isEmpty()) << errorsB.join(QStringLiteral("; ")).toStdString();

    const QList<QCameraDevice> cameras = enumerator.cameras();
    if (cameras.isEmpty()) {
        GTEST_LOG_(INFO) << "No camera available — skipping the video-renegotiation part of this test.";
    } else {
        // Adding a video track to an already-negotiated connection
        // (issue #72) triggers OnRenegotiationNeeded() -> negotiateLocal()
        // -> a fresh offer/answer round trip through the same live
        // chat-service relay — verifies that path works end to end, not
        // just the initial join negotiation covered above.
        //
        // issue #91: negotiation succeeding isn't proof B actually
        // decodes A's video — PeerObserver::OnTrack() /
        // RemoteVideoSink::OnFrame() need to be exercised for real, so
        // this also waits for at least one decoded frame on B's side.
        QImage receivedFrame;
        QObject::connect(&callManagerB, &CallManager::remoteVideoFrameReceived,
                          [&](const QString& login, const QImage& frame) {
                              if (login == loginA) {
                                  receivedFrame = frame;
                              }
                          });

        callManagerA.enableVideo(cameras.first());
        {
            QEventLoop loop;
            QTimer::singleShot(3000, &loop, &QEventLoop::quit);
            loop.exec();
        }
        EXPECT_TRUE(callManagerA.videoEnabled());
        EXPECT_TRUE(errorsA.isEmpty()) << errorsA.join(QStringLiteral("; ")).toStdString();
        EXPECT_TRUE(errorsB.isEmpty()) << errorsB.join(QStringLiteral("; ")).toStdString();
        EXPECT_FALSE(receivedFrame.isNull()) << "B never decoded a video frame from A";
        if (!receivedFrame.isNull()) {
            EXPECT_GT(receivedFrame.width(), 0);
            EXPECT_GT(receivedFrame.height(), 0);
        }

        callManagerA.disableVideo();
        {
            QEventLoop loop;
            QTimer::singleShot(1000, &loop, &QEventLoop::quit);
            loop.exec();
        }
        EXPECT_FALSE(callManagerA.videoEnabled());
    }

    bool aSawBLeave = false;
    QObject::connect(&callManagerA, &CallManager::participantLeft, [&](const QString& login) {
        if (login == loginB) {
            aSawBLeave = true;
        }
    });

    callManagerB.leaveCall();

    {
        QEventLoop loop;
        QTimer::singleShot(2000, &loop, &QEventLoop::quit);
        QObject::connect(&callManagerA, &CallManager::participantLeft, &loop, [&](const QString&) { loop.quit(); });
        loop.exec();
    }
    EXPECT_TRUE(aSawBLeave);

    callManagerA.leaveCall();
}

}  // namespace devicehub
