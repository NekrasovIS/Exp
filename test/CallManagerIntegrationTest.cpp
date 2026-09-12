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

/// Прокачивает событийный цикл, пока @p predicate не станет true или не
/// истечёт @p timeoutMs (issue #233) — полный неполиморфный сбор
/// ICE-кандидатов (non-trickle SFU-протокол, см. doc-комментарий
/// PeerObserver::OnIceGatheringChange()) на сети с недоступным STUN
/// (см. лог "UDP send ... failed") занимает заметно дольше, чем на
/// чистой сети, так что фиксированный QTimer::singleShot() либо
/// избыточно долгий на быстрой сети, либо слишком короткий на
/// медленной — polling с щедрым верхним пределом устойчив к обоим
/// случаям, возвращаясь сразу, как только @p predicate выполнится.
template <typename Predicate>
bool waitUntil(Predicate predicate, int timeoutMs = 30000, int pollIntervalMs = 200) {
    QEventLoop loop;
    QTimer poller;
    QTimer::singleShot(timeoutMs, &loop, &QEventLoop::quit);
    QObject::connect(&poller, &QTimer::timeout, &loop, [&]() {
        if (predicate()) {
            loop.quit();
        }
    });
    poller.start(pollIntervalMs);
    if (predicate()) {
        return true;
    }
    loop.exec();
    return predicate();
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

// Issue #362 — chat-service отвечает на СОБСТВЕННЫЙ call_join
// вызывающего начальным ростером уже присутствующих участников
// (call_roster), отдельно от call_peer_joined, которым уведомляет
// остальных о новом присоединившемся. Ни один клиент раньше не слушал
// callRosterReceived вообще — участник, зашедший не первым, никогда не
// узнавал о тех, кто уже был в звонке. Не требует Janus (сам ростер от
// него не зависит) и не требует реальных аудио-устройств (joinCall()
// с нулевыми QAudioDevice всё равно шлёт call_join, см. её же
// doc-комментарий и SFU-тесты ниже).
TEST(CallManagerIntegrationTest, SecondParticipantLearnsAboutFirstFromTheInitialRoster) {
    const QUrl authUrl(QString::fromStdString(envOrDefault("AUTH_SERVICE_URL", "http://127.0.0.1:8080")));
    const QUrl userUrl(QString::fromStdString(envOrDefault("USER_SERVICE_URL", "http://127.0.0.1:8081")));
    const QUrl chatRestUrl(QString::fromStdString(envOrDefault("CHAT_SERVICE_URL", "http://127.0.0.1:8082")));
    const QUrl chatWsUrl(QString::fromStdString(envOrDefault("CHAT_SERVICE_WS_URL", "ws://127.0.0.1:8083")));

    const qint64 suffix = QDateTime::currentMSecsSinceEpoch();
    const QString loginA = QStringLiteral("call-roster-test-a-%1").arg(suffix);
    const QString loginB = QStringLiteral("call-roster-test-b-%1").arg(suffix);
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
        chatRestClient.createCommunity(*tokenA, QStringLiteral("call-roster-test"));
        loop.exec();
    }
    ASSERT_GT(communityId, 0);

    {
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

    // A joins first — nobody else is in the call yet, so A's own roster
    // is empty (nothing to assert there); B joins second and should
    // learn about A purely from B's own call_join response. Waits for
    // chat-service to actually acknowledge A's own join (ChatClient's
    // own callRosterReceived(), independent of whether CallManager
    // listens to it — the very thing under test) before letting B join,
    // otherwise the two joinCall()s could race and B's join might reach
    // the server before A's does, making B's (legitimately) empty
    // roster look like a false pass.
    bool aRosterAcked = false;
    QObject::connect(&chatClientA, &ChatClient::callRosterReceived, &chatClientA,
                      [&](const QStringList&) { aRosterAcked = true; });
    callManagerA.joinCall(QAudioDevice(), QAudioDevice());
    ASSERT_TRUE(waitUntil([&]() { return aRosterAcked; }, /*timeoutMs=*/5000))
        << "A's own call_join was never acknowledged by chat-service";

    QStringList participantsSeenByB;
    QObject::connect(&callManagerB, &CallManager::participantJoined, &callManagerB,
                      [&](const QString& login) { participantsSeenByB << login; });

    callManagerB.joinCall(QAudioDevice(), QAudioDevice());
    ASSERT_TRUE(waitUntil([&]() { return !participantsSeenByB.isEmpty(); }, /*timeoutMs=*/5000))
        << "B's CallManager never emitted participantJoined for A, who was already in the call";
    EXPECT_TRUE(participantsSeenByB.contains(loginA));

    callManagerA.leaveCall();
    callManagerB.leaveCall();
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

// issue #233: #232 заменил mesh на SFU, но собственное условие issue #233
// на удаление mesh-кода — не "теоретическая замена", а подтверждённый
// работающий SFU-звонок минимум с 3 участниками (тем количеством, где
// mesh уже начинал бы давать нагрузку). Тест выше проверяет только двух —
// этот специально добавляет третьего. Как и тест выше, не дожидается
// полной связности ICE/DTLS (см. её doc-комментарий) — сигналинг-уровень
// (Janus должен знать о трёх publisher'ах в комнате) проверяется через
// ASSERT, реальные subscribe-соединения на стороне клиента (peers_) —
// через EXPECT, поскольку зависят от реальной ICE/DTLS-связности, которая
// на изолированной/ограниченной сети может не устанавливаться вовсе.
// Основное ручное подтверждение самого протокола для #233 (реальные
// ICE/DTLS+SDP, без этого ограничения) — services/janus/verify/
// verify-forwarding.mjs, запускаемый в docker-сети Janus, не с хоста.
//
// Все три участника здесь намеренно подключаются с null-устройствами
// ввода/вывода звука (см. joinCall() ниже), а не с реальным устройством,
// как сестринский двухучастниковый тест выше: три одновременно живых
// PeerConnectionFactory (с настоящими worker/network/signaling-потоками
// WebRTC) в одном процессе на этой машине вносят достаточную задержку
// планирования потоков, чтобы буфер захвата с реального микрофона у A
// или B иногда приходил за 20мс вместо ожидаемых 10мс;
// CallAudioDeviceModule::pushCapturedAudio() передаёт его в WebRTC как
// есть, а собственный AudioTransportImpl WebRTC жёстко проверяет
// (фатальный CHECK, валит весь процесс, а не просто один тест) кратность
// 10мс — воспроизводимо начиная именно с третьего одновременного
// реального захвата, не в коде CallManager из этого коммита. joinCall()
// уже штатно поддерживает null-устройство (см. его же doc-комментарий) —
// сигналинг, ICE/DTLS и реальный SDP-обмен продолжаются как обычно,
// теряется только реальный захват/рендер звука, что здесь и не
// проверяется (см. выше — за это отвечает verify-forwarding.mjs).
TEST(CallManagerIntegrationTest, SfuThreeParticipantPublishAndSubscribeRoundTrip) {
    const QUrl authUrl(QString::fromStdString(envOrDefault("AUTH_SERVICE_URL", "http://127.0.0.1:8080")));
    const QUrl userUrl(QString::fromStdString(envOrDefault("USER_SERVICE_URL", "http://127.0.0.1:8081")));
    const QUrl chatRestUrl(QString::fromStdString(envOrDefault("CHAT_SERVICE_URL", "http://127.0.0.1:8082")));
    const QUrl chatWsUrl(QString::fromStdString(envOrDefault("CHAT_SERVICE_WS_URL", "ws://127.0.0.1:8083")));
    const QUrl janusUrl(QString::fromStdString(envOrDefault("JANUS_URL", "http://127.0.0.1:8088/janus")));

    const qint64 suffix = QDateTime::currentMSecsSinceEpoch();
    const QString loginA = QStringLiteral("sfu-3test-a-%1").arg(suffix);
    const QString loginB = QStringLiteral("sfu-3test-b-%1").arg(suffix);
    const QString loginC = QStringLiteral("sfu-3test-c-%1").arg(suffix);
    const QString password = QStringLiteral("integration-test-password");

    QNetworkAccessManager manager;
    if (!registerTestUser(manager, userUrl, loginA, password) ||
        !registerTestUser(manager, userUrl, loginB, password) ||
        !registerTestUser(manager, userUrl, loginC, password)) {
        GTEST_SKIP() << "user-service not reachable — start the full stack to run this test.";
    }

    AuthClient authClientA(authUrl);
    AuthClient authClientB(authUrl);
    AuthClient authClientC(authUrl);
    const std::optional<QString> tokenA = requestToken(authClientA, loginA, password);
    const std::optional<QString> tokenB = requestToken(authClientB, loginB, password);
    const std::optional<QString> tokenC = requestToken(authClientC, loginC, password);
    if (!tokenA.has_value() || !tokenB.has_value() || !tokenC.has_value()) {
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
        chatRestClient.createCommunity(*tokenA, QStringLiteral("sfu-3participant-test"));
        loop.exec();
    }
    ASSERT_GT(communityId, 0);

    for (const QString& token : {*tokenB, *tokenC}) {
        QEventLoop loop;
        QTimer::singleShot(3000, &loop, &QEventLoop::quit);
        QObject::connect(&chatRestClient, &ChatRestClient::communityJoined, &loop, [&](qint64) { loop.quit(); });
        chatRestClient.joinCommunity(token, communityId);
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
    if (postJsonSync(manager, janusUrl, QJsonObject{{"janus", "create"}}).value(QStringLiteral("janus")).toString() !=
        QStringLiteral("success")) {
        GTEST_SKIP() << "Janus not reachable — run `docker compose --profile sfu up -d janus` to run this test.";
    }

    ChatClient chatClientA(chatWsUrl);
    ChatClient chatClientB(chatWsUrl);
    ChatClient chatClientC(chatWsUrl);
    for (auto* client : {&chatClientA, &chatClientB, &chatClientC}) {
        const QString& token = client == &chatClientA ? *tokenA : (client == &chatClientB ? *tokenB : *tokenC);
        bool subscribed = false;
        QEventLoop loop;
        QTimer::singleShot(3000, &loop, &QEventLoop::quit);
        QObject::connect(client, &ChatClient::subscribed, &loop, [&](qint64) {
            subscribed = true;
            loop.quit();
        });
        client->connectToChannel(token, channelId);
        loop.exec();
        ASSERT_TRUE(subscribed);
    }

    AudioInputDevice audioInputA;
    AudioOutputDevice audioOutputA;
    AudioInputDevice audioInputB;
    AudioOutputDevice audioOutputB;
    AudioInputDevice audioInputC;
    AudioOutputDevice audioOutputC;
    CameraDevice cameraA;
    CameraDevice cameraB;
    CameraDevice cameraC;
    ScreenCaptureDevice screenCaptureA;
    ScreenCaptureDevice screenCaptureB;
    ScreenCaptureDevice screenCaptureC;

    CallManager callManagerA(chatClientA, audioInputA, audioOutputA, cameraA, screenCaptureA);
    CallManager callManagerB(chatClientB, audioInputB, audioOutputB, cameraB, screenCaptureB);
    CallManager callManagerC(chatClientC, audioInputC, audioOutputC, cameraC, screenCaptureC);
    callManagerA.setLocalLogin(loginA);
    callManagerB.setLocalLogin(loginB);
    callManagerC.setLocalLogin(loginC);

    QStringList errorsA;
    QStringList errorsB;
    QStringList errorsC;
    QObject::connect(&callManagerA, &CallManager::callError, [&](const QString& message) { errorsA << message; });
    QObject::connect(&callManagerB, &CallManager::callError, [&](const QString& message) { errorsB << message; });
    QObject::connect(&callManagerC, &CallManager::callError, [&](const QString& message) { errorsC << message; });

    callManagerA.joinCall(QAudioDevice(), QAudioDevice());
    // A публикуется один — ждём, пока Janus реально не увидит его
    // publisher'ом (не просто "запрос на join отправлен"), прежде чем
    // впустить кого-то ещё: полный сбор ICE-кандидатов (non-trickle
    // SFU-протокол) на сети без доступного STUN может занимать заметно
    // дольше пары секунд, см. doc-комментарий waitUntil() выше.
    ASSERT_TRUE(waitUntil([&]() {
        const std::optional<QJsonArray> participants = listJanusRoomParticipants(manager, janusUrl, room);
        return participants.has_value() && !participants->isEmpty();
    })) << "A never became a publisher in Janus's own bookkeeping";

    callManagerB.joinCall(QAudioDevice(), QAudioDevice());
    // B видит A уже publisher'ом (1 subscribe) и сам публикуется
    // (1 publish); A получает уведомление о новом publisher'е B и
    // подписывается на него (1 subscribe) — три раунда offer/answer,
    // каждый со своим полным сбором ICE-кандидатов. Не ASSERT — как и
    // сигналинговый раунд-трип в SfuPublishAndSubscribeOfferAnswerRoundTrip
    // выше, реальное подключение subscribe-соединений зависит от полной
    // связности ICE/DTLS с Janus, которая на изолированной сети (в т.ч.
    // сети CI) может не устанавливаться вовсе — это здесь не проверяется
    // (см. doc-комментарий класса теста), только даём процессу шанс
    // устояться перед финальными проверками ниже.
    waitUntil([&]() { return callManagerA.activeRemotePeerCount() >= 1 && callManagerB.activeRemotePeerCount() >= 1; });

    callManagerC.joinCall(QAudioDevice(), QAudioDevice());
    // C видит и A, и B уже publisher'ами (2 subscribe) и сам
    // публикуется (1 publish); A и B каждый получает уведомление о
    // новом publisher'е C и подписывается на него (ещё 2 subscribe) —
    // пять независимых раундов offer/answer, самая нагруженная фаза теста.
    waitUntil([&]() {
        return callManagerA.activeRemotePeerCount() >= 2 && callManagerB.activeRemotePeerCount() >= 2 &&
               callManagerC.activeRemotePeerCount() >= 2;
    });

    // Все три ожидаемо получают по два callError о null-устройствах
    // ввода/вывода звука (см. их же joinCall() выше и doc-комментарий
    // класса теста) — это не ошибка теста, поэтому здесь не EXPECT_TRUE
    // на пустоту, а точная проверка именно этих двух сообщений у каждого.
    EXPECT_EQ(errorsA.size(), 2) << errorsA.join(QStringLiteral("; ")).toStdString();
    EXPECT_EQ(errorsB.size(), 2) << errorsB.join(QStringLiteral("; ")).toStdString();
    EXPECT_EQ(errorsC.size(), 2) << errorsC.join(QStringLiteral("; ")).toStdString();

    // Сигналинг-уровень (не требует полной связности ICE/DTLS, см. выше) —
    // тот же самый Janus REST listparticipants, что и в двухучастниковом
    // тесте, ровно тем же ASSERT'ом: Janus должен видеть все три join'а
    // как реальных publisher'ов комнаты.
    const std::optional<QJsonArray> participants = listJanusRoomParticipants(manager, janusUrl, room);
    ASSERT_TRUE(participants.has_value()) << "Janus became unreachable mid-test";
    ASSERT_EQ(participants->size(), 3) << "expected exactly A, B and C to be publishers in the room";
    QSet<QString> displays;
    for (const QJsonValue& participant : *participants) {
        displays.insert(participant.toObject().value(QStringLiteral("display")).toString());
    }
    EXPECT_TRUE(displays.contains(loginA));
    EXPECT_TRUE(displays.contains(loginB));
    EXPECT_TRUE(displays.contains(loginC));
    // Реальные subscribe-соединения (peers_/activeRemotePeerCount())
    // сознательно не проверяются здесь тем же ASSERT/EXPECT, что и Janus
    // выше — как и сестринский двухучастниковый тест выше (см. её
    // финальные проверки), это зависит от полной связности ICE/DTLS, а
    // не только от сигналинга, и на изолированной/ограниченной сети
    // может не устанавливаться вовсе (см. doc-комментарий класса теста и
    // waitUntil() выше); реальный forwarding-путь подтверждает
    // verify-forwarding.mjs.

    callManagerA.leaveCall();
    callManagerB.leaveCall();
    callManagerC.leaveCall();
}

}  // namespace devicehub
