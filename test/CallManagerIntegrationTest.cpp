#include "auth/AuthClient.h"
#include "chat/CallManager.h"
#include "chat/ChatClient.h"
#include "chat/ChatRestClient.h"
#include "devices/AudioInputDevice.h"
#include "devices/AudioOutputDevice.h"
#include "devices/CameraDevice.h"
#include "devices/DeviceEnumerator.h"
#include "devices/ScreenCaptureDevice.h"

#include <gtest/gtest.h>

#include <QAudioDevice>
#include <QDateTime>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSet>
#include <QTimer>
#include <QUrl>

#include <cstdlib>

// Требует полного живого стека: auth-service, user-service, chat-service
// (REST + WebSocket) и оба экземпляра Postgres (см. docker-compose.yml), а
// также реальный доступ в сеть для сбора ICE-кандидатов. Пропускает себя
// вместо падения, если стек не запущен — та же конвенция, что и в
// ChatClientIntegrationTest.
//
// Этот тест прогоняет реальный цикл обмена offer/answer/сигналинга
// (генерацию SDP, ретрансляцию через живой chat-service, разбор,
// авто-ответ) через CallManager::callError — он НЕ дожидается полной
// связности ICE/DTLS, поскольку она зависит от доступности STUN и таймингов,
// которые сделали бы тест нестабильным (flaky) в изолированном окружении
// CI; проверяется корректность именно той части сигналинга, что реально
// реализована в данном изменении.

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

/// Один синхронный POST JSON-запрос (issue #232) — тот же паттерн
/// "QEventLoop + QTimer::singleShot тайм-аут", что уже использует
/// registerTestUser()/requestToken() выше, только обобщённый под
/// произвольное тело/URL, поскольку нужен для нескольких разных запросов
/// к plain HTTP API Janus. Пустой QJsonObject при любой сетевой ошибке
/// или тайм-ауте — вызывающая сторона трактует это как "недоступно".
QJsonObject postJsonSync(QNetworkAccessManager& manager, const QUrl& url, const QJsonObject& body,
                          int timeoutMs = 3000) {
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    // Janus требует "transaction" в каждом запросе (иначе error 456
    // "Missing mandatory element") — не секрет/идентификатор, только
    // корреляция запрос/ответ, поэтому годится и метка времени, тот же
    // подход, что и у JanusClient (chat-service) для того же протокола.
    QJsonObject bodyWithTransaction = body;
    bodyWithTransaction.insert(QStringLiteral("transaction"),
                                QString::number(QDateTime::currentMSecsSinceEpoch()));
    QNetworkReply* reply = manager.post(request, QJsonDocument(bodyWithTransaction).toJson(QJsonDocument::Compact));

    QJsonObject result;
    QEventLoop loop;
    QTimer::singleShot(timeoutMs, &loop, &QEventLoop::quit);
    QObject::connect(reply, &QNetworkReply::finished, &loop, [&]() {
        if (reply->error() == QNetworkReply::NoError) {
            result = QJsonDocument::fromJson(reply->readAll()).object();
        }
        reply->deleteLater();
        loop.quit();
    });
    loop.exec();
    return result;
}

/// Список участников videoroom-комнаты @p room в Janus (запрос
/// "listparticipants", тот же протокол, что и у JanusClient/services/
/// janus/verify/verify-forwarding.mjs — здесь напрямую, а не через
/// chat-service, потому что это верификация со стороны теста, а не часть
/// самого прокси) — std::nullopt, если Janus недоступен по любой причине
/// на любом из трёх шагов (create/attach/message).
std::optional<QJsonArray> listJanusRoomParticipants(QNetworkAccessManager& manager, const QUrl& janusUrl,
                                                      const QString& room) {
    const QJsonObject sessionResponse = postJsonSync(manager, janusUrl, QJsonObject{{"janus", "create"}});
    if (sessionResponse.value(QStringLiteral("janus")).toString() != QStringLiteral("success")) {
        return std::nullopt;
    }
    const qint64 sessionId = sessionResponse.value(QStringLiteral("data")).toObject().value(QStringLiteral("id")).toVariant().toLongLong();

    const QJsonObject attachResponse =
        postJsonSync(manager, QUrl(janusUrl.toString() + QStringLiteral("/%1").arg(sessionId)),
                     QJsonObject{{"janus", "attach"}, {"plugin", "janus.plugin.videoroom"}});
    if (attachResponse.value(QStringLiteral("janus")).toString() != QStringLiteral("success")) {
        return std::nullopt;
    }
    const qint64 handleId = attachResponse.value(QStringLiteral("data")).toObject().value(QStringLiteral("id")).toVariant().toLongLong();

    const QJsonObject listResponse = postJsonSync(
        manager, QUrl(janusUrl.toString() + QStringLiteral("/%1/%2").arg(sessionId).arg(handleId)),
        QJsonObject{{"janus", "message"}, {"body", QJsonObject{{"request", "listparticipants"}, {"room", room}}}});
    const QJsonObject data = listResponse.value(QStringLiteral("plugindata")).toObject().value(QStringLiteral("data")).toObject();
    if (data.value(QStringLiteral("videoroom")).toString() != QStringLiteral("participants")) {
        return std::nullopt;
    }
    return data.value(QStringLiteral("participants")).toArray();
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

    {
        // issue #231: call_join теперь проверяет членство в сообществе —
        // A автоматически стал участником, создав его, но B должен
        // вступить явно, иначе его собственный call_join будет отклонён
        // (тихо для CallManager: ChatClient::errorOccurred() при таком
        // отказе никто здесь не слушает, поэтому без этого шага тест
        // выглядел бы "прошедшим", просто ничего реально не согласовывая).
        QEventLoop loop;
        QTimer::singleShot(3000, &loop, &QEventLoop::quit);
        QObject::connect(&chatRestClient, &ChatRestClient::communityJoined, &loop, [&](qint64) { loop.quit(); });
        chatRestClient.joinCommunity(*tokenB, communityId);
        loop.exec();
    }

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

    // Реальные перечисленные устройства, а не QAudioDevice, созданный по
    // умолчанию — CallManager::joinCall() трактует нулевое устройство как
    // «ничего не выбрано» и пропускает его с callError() вместо падения, что
    // заставило бы этот тест проходить по неверной причине (реально не
    // проверяя ту работу с реальными устройствами, ради которой тест и
    // задуман).
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
    ScreenCaptureDevice screenCaptureA;
    ScreenCaptureDevice screenCaptureB;

    CallManager callManagerA(chatClientA, audioInputA, audioOutputA, cameraA, screenCaptureA);
    CallManager callManagerB(chatClientB, audioInputB, audioOutputB, cameraB, screenCaptureB);
    // issue #232: без этого CallManager не сможет объявить Janus'у, кто
    // публикует (поле "display" при join как publisher).
    callManagerA.setLocalLogin(loginA);
    callManagerB.setLocalLogin(loginB);

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
        // A присоединился один (пустой список участников) — согласовывать
        // пока нечего, просто даём кадру присоединения время дойти туда и
        // обратно, прежде чем присоединится B.
        QEventLoop loop;
        QTimer::singleShot(500, &loop, &QEventLoop::quit);
        loop.exec();
    }

    callManagerB.joinCall(inputDevice, outputDevice);

    {
        // Ждём, пока устаканится обмен offer/answer/сигналинга (ретранслируемый
        // через живой chat-service).
        QEventLoop loop;
        QTimer::singleShot(4000, &loop, &QEventLoop::quit);
        loop.exec();
    }

    EXPECT_TRUE(aSawBJoin);
    EXPECT_TRUE(errorsA.isEmpty()) << errorsA.join(QStringLiteral("; ")).toStdString();
    EXPECT_TRUE(errorsB.isEmpty()) << errorsB.join(QStringLiteral("; ")).toStdString();

    // issue #232: видео теперь идёт через SFU (publish/subscribe к
    // Janus), а не напрямую пиру, как раньше через mesh. Этот блок
    // (issue #72/#91/#185) проверял renegotiation ПОСЛЕ того, как обе
    // стороны уже согласовались, добавляя видео уже устоявшемуся
    // mesh-соединению — но Janus не проталкивает новый поток уже
    // подписавшимся подписчикам автоматически (подписчик должен сам
    // запросить его отдельным "subscribe" на уже открытый handle, что
    // CallManager пока не реализует, только исходную подписку в момент
    // join, см. её doc-комментарий), поэтому здесь сейчас нет корректного
    // эквивалента и сценарий убран, а не подменён похожим, но неверным.
    // То, что сам SFU-путь (publish+subscribe через Janus) реально
    // работает, проверяет SfuPublishAndSubscribeOfferAnswerRoundTrip ниже
    // (на аудио — камера/демонстрация экрана через SFU не покрыты ни там,
    // ни здесь).

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

// issue #232: тот же уровень строгости, что и у mesh-теста выше — не
// дожидается полной ICE/DTLS-связности (см. её doc-комментарий), только
// проверяет, что сам обмен offer/answer/jsep с Janus проходит без ошибок
// и оба участника реально появляются в комнате как publisher-ы (а не
// просто "не упало"): join публикующего handle'а + configure(offer) для
// каждого участника, join подписывающего handle'а + start(answer) для
// подписки каждого на другого — четыре независимых offer/answer раунда
// на пару участников (issue #232's "к publish- и subscribe-соединениям").
TEST(CallManagerIntegrationTest, SfuPublishAndSubscribeOfferAnswerRoundTrip) {
    const QUrl authUrl(QString::fromStdString(envOrDefault("AUTH_SERVICE_URL", "http://127.0.0.1:8080")));
    const QUrl userUrl(QString::fromStdString(envOrDefault("USER_SERVICE_URL", "http://127.0.0.1:8081")));
    const QUrl chatRestUrl(QString::fromStdString(envOrDefault("CHAT_SERVICE_URL", "http://127.0.0.1:8082")));
    const QUrl chatWsUrl(QString::fromStdString(envOrDefault("CHAT_SERVICE_WS_URL", "ws://127.0.0.1:8083")));
    const QUrl janusUrl(QString::fromStdString(envOrDefault("JANUS_URL", "http://127.0.0.1:8088/janus")));

    const qint64 suffix = QDateTime::currentMSecsSinceEpoch();
    const QString loginA = QStringLiteral("sfu-test-a-%1").arg(suffix);
    const QString loginB = QStringLiteral("sfu-test-b-%1").arg(suffix);
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
        chatRestClient.createCommunity(*tokenA, QStringLiteral("sfu-integration-test"));
        loop.exec();
    }
    ASSERT_GT(communityId, 0);

    {
        // issue #231: B должен явно вступить, иначе его call_join
        // отклоняется проверкой членства — см. тот же комментарий в
        // OfferAnswerSignalingRoundTripWithoutErrors выше.
        QEventLoop loop;
        QTimer::singleShot(3000, &loop, &QEventLoop::quit);
        QObject::connect(&chatRestClient, &ChatRestClient::communityJoined, &loop, [&](qint64) { loop.quit(); });
        chatRestClient.joinCommunity(*tokenB, communityId);
        loop.exec();
    }

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

    const QString room = QStringLiteral("channel-%1").arg(channelId);
    // Голая проверка досягаемости Janus — не listJanusRoomParticipants()
    // для этой ещё не существующей комнаты: она создаётся только позже,
    // самим call_join (issue #231's ensureRoomExists()), поэтому здесь
    // "комнаты пока нет" неотличимо от "Janus недоступен" и ложно
    // пропустило бы тест даже при поднятом Janus.
    if (postJsonSync(manager, janusUrl, QJsonObject{{"janus", "create"}}).value(QStringLiteral("janus")).toString() !=
        QStringLiteral("success")) {
        GTEST_SKIP() << "Janus not reachable — run `docker compose --profile sfu up -d janus` to run this test.";
    }

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
    ScreenCaptureDevice screenCaptureA;
    ScreenCaptureDevice screenCaptureB;

    CallManager callManagerA(chatClientA, audioInputA, audioOutputA, cameraA, screenCaptureA);
    CallManager callManagerB(chatClientB, audioInputB, audioOutputB, cameraB, screenCaptureB);
    callManagerA.setLocalLogin(loginA);
    callManagerB.setLocalLogin(loginB);

    QStringList errorsA;
    QStringList errorsB;
    QObject::connect(&callManagerA, &CallManager::callError, [&](const QString& message) { errorsA << message; });
    QObject::connect(&callManagerB, &CallManager::callError, [&](const QString& message) { errorsB << message; });

    callManagerA.joinCall(inputDevice, outputDevice);
    {
        // Даём A время самому опубликоваться (join + полный сбор ICE +
        // configure(offer) + jsep-answer — эта реализация ждёт полного
        // сбора кандидатов перед отправкой, а не трикклит их по одному,
        // см. doc-комментарий PeerObserver::OnIceGatheringChange(), так
        // что это не мгновенно), прежде чем присоединится B — так B
        // увидит A уже в списке publishers и сразу же начнёт на него
        // подписываться, ровно как и в mesh-тесте выше с ростером.
        QEventLoop loop;
        QTimer::singleShot(4000, &loop, &QEventLoop::quit);
        loop.exec();
    }
    callManagerB.joinCall(inputDevice, outputDevice);
    {
        // Обе публикации + обе взаимные подписки — четыре независимых
        // offer/answer раунда через Janus (см. doc-комментарий теста), у
        // каждого свой полный сбор ICE-кандидатов.
        QEventLoop loop;
        QTimer::singleShot(8000, &loop, &QEventLoop::quit);
        loop.exec();
    }

    EXPECT_TRUE(errorsA.isEmpty()) << errorsA.join(QStringLiteral("; ")).toStdString();
    EXPECT_TRUE(errorsB.isEmpty()) << errorsB.join(QStringLiteral("; ")).toStdString();

    const std::optional<QJsonArray> participants = listJanusRoomParticipants(manager, janusUrl, room);
    ASSERT_TRUE(participants.has_value()) << "Janus became unreachable mid-test";
    ASSERT_EQ(participants->size(), 2) << "expected exactly A and B to be publishers in the room";
    QSet<QString> displays;
    for (const QJsonValue& participant : *participants) {
        displays.insert(participant.toObject().value(QStringLiteral("display")).toString());
    }
    EXPECT_TRUE(displays.contains(loginA));
    EXPECT_TRUE(displays.contains(loginB));

    callManagerA.leaveCall();
    callManagerB.leaveCall();
}

}  // namespace devicehub
