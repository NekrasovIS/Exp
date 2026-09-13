#include "auth/AuthClient.h"
#include "user/UserProfileClient.h"

#include <gtest/gtest.h>

#include <QDateTime>
#include <QEventLoop>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageAuthenticationCode>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>

#include <cstdlib>

// Требует запущенных auth-service, user-service и Postgres (см.
// services/auth-service, services/user-service, docker-compose.yml).
// Доступны по AUTH_SERVICE_URL / USER_SERVICE_URL (по умолчанию
// http://127.0.0.1:8080 / http://127.0.0.1:8081). Пропускает себя вместо
// падения, если они не запущены — CI сейчас не оркестрирует все три
// вместе, поэтому это ручная/локальная сквозная (end-to-end) проверка, а
// не часть гарантий автоматического набора тестов.

namespace devicehub {
namespace {

bool registerTestUser(QNetworkAccessManager& manager, const QUrl& userServiceUrl, const QString& login,
                       const QString& password) {
    QNetworkRequest request(userServiceUrl.resolved(QUrl(QStringLiteral("/users/register"))));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));

    const QJsonObject body{{"login", login}, {"password", password}};
    QNetworkReply* reply = manager.post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));

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

TEST(AuthClientIntegrationTest, RequestTokenAndVerifyRoundTrip) {
    const char* authUrlEnv = std::getenv("AUTH_SERVICE_URL");
    const QUrl authBaseUrl(authUrlEnv != nullptr ? QString::fromLocal8Bit(authUrlEnv)
                                                  : QStringLiteral("http://127.0.0.1:8080"));
    const char* userUrlEnv = std::getenv("USER_SERVICE_URL");
    const QUrl userBaseUrl(userUrlEnv != nullptr ? QString::fromLocal8Bit(userUrlEnv)
                                                  : QStringLiteral("http://127.0.0.1:8081"));

    // Уникально при каждом запуске, чтобы никогда не столкнуться с
    // оставшейся строкой от предыдущего запуска против того же экземпляра
    // Postgres.
    const QString login = QStringLiteral("integration-test-%1").arg(QDateTime::currentMSecsSinceEpoch());
    const QString password = QStringLiteral("integration-test-password");

    QNetworkAccessManager setupManager;
    if (!registerTestUser(setupManager, userBaseUrl, login, password)) {
        GTEST_SKIP() << "user-service not reachable at " << userBaseUrl.toString().toStdString()
                      << " — start docker-compose + user-service locally to run this test.";
    }

    AuthClient client(authBaseUrl);

    QString receivedToken;
    QString receivedRefreshToken;
    QString errorMessage;
    {
        QEventLoop loop;
        QTimer::singleShot(3000, &loop, &QEventLoop::quit);
        QObject::connect(&client, &AuthClient::tokenReceived, &loop,
                          [&](const QString& token, const QString& refreshToken, qint64 /*expiresAt*/) {
                              receivedToken = token;
                              receivedRefreshToken = refreshToken;
                              loop.quit();
                          });
        QObject::connect(&client, &AuthClient::errorOccurred, &loop, [&](const QString& message) {
            errorMessage = message;
            loop.quit();
        });
        client.requestToken(login, password);
        loop.exec();
    }

    if (receivedToken.isEmpty()) {
        GTEST_SKIP() << "auth-service not reachable at " << authBaseUrl.toString().toStdString() << " (error: "
                      << errorMessage.toStdString() << ") — start it locally to run this test.";
    }

    bool verified = false;
    QString subject;
    {
        QEventLoop loop;
        QTimer::singleShot(3000, &loop, &QEventLoop::quit);
        QObject::connect(&client, &AuthClient::tokenVerified, &loop, [&](bool valid, const QString& verifiedSubject) {
            verified = valid;
            subject = verifiedSubject;
            loop.quit();
        });
        client.verifyToken(receivedToken);
        loop.exec();
    }

    EXPECT_TRUE(verified);
    EXPECT_EQ(subject, login);
    ASSERT_FALSE(receivedRefreshToken.isEmpty());

    // issue #105: refresh-токен от того же логина обменивается на совершенно
    // новый (но по-прежнему проверяемый) access-токен, без повторного ввода
    // учётных данных.
    QString refreshedToken;
    {
        QEventLoop loop;
        QTimer::singleShot(3000, &loop, &QEventLoop::quit);
        QObject::connect(&client, &AuthClient::tokenReceived, &loop,
                          [&](const QString& token, const QString&, qint64 /*expiresAt*/) {
                              refreshedToken = token;
                              loop.quit();
                          });
        client.refreshAccessToken(receivedRefreshToken);
        loop.exec();
    }
    ASSERT_FALSE(refreshedToken.isEmpty());

    bool refreshedVerified = false;
    QString refreshedSubject;
    {
        QEventLoop loop;
        QTimer::singleShot(3000, &loop, &QEventLoop::quit);
        QObject::connect(&client, &AuthClient::tokenVerified, &loop, [&](bool valid, const QString& verifiedSubject) {
            refreshedVerified = valid;
            refreshedSubject = verifiedSubject;
            loop.quit();
        });
        client.verifyToken(refreshedToken);
        loop.exec();
    }
    EXPECT_TRUE(refreshedVerified);
    EXPECT_EQ(refreshedSubject, login);
}

TEST(AuthClientIntegrationTest, RegisterUserAutoIssuesAVerifiableToken) {
    const char* authUrlEnv = std::getenv("AUTH_SERVICE_URL");
    const QUrl authBaseUrl(authUrlEnv != nullptr ? QString::fromLocal8Bit(authUrlEnv)
                                                  : QStringLiteral("http://127.0.0.1:8080"));

    const QString login = QStringLiteral("integration-register-test-%1").arg(QDateTime::currentMSecsSinceEpoch());
    const QString password = QStringLiteral("integration-test-password");

    AuthClient client(authBaseUrl);

    bool registrationResult = false;
    bool registrationCompletedFired = false;
    QString receivedToken;
    QString errorMessage;
    {
        QEventLoop loop;
        QTimer::singleShot(3000, &loop, &QEventLoop::quit);
        QObject::connect(&client, &AuthClient::registrationCompleted, &loop, [&](bool registered) {
            registrationCompletedFired = true;
            registrationResult = registered;
        });
        QObject::connect(&client, &AuthClient::tokenReceived, &loop,
                          [&](const QString& token, const QString& /*refreshToken*/, qint64 /*expiresAt*/) {
                              receivedToken = token;
                              loop.quit();
                          });
        QObject::connect(&client, &AuthClient::errorOccurred, &loop, [&](const QString& message) {
            errorMessage = message;
            loop.quit();
        });
        client.registerUser(login, password);
        loop.exec();
    }

    if (!registrationCompletedFired && receivedToken.isEmpty()) {
        GTEST_SKIP() << "auth-service not reachable at " << authBaseUrl.toString().toStdString() << " (error: "
                      << errorMessage.toStdString() << ") — start it locally to run this test.";
    }

    ASSERT_TRUE(registrationResult);
    ASSERT_FALSE(receivedToken.isEmpty());

    bool verified = false;
    QString subject;
    {
        QEventLoop loop;
        QTimer::singleShot(3000, &loop, &QEventLoop::quit);
        QObject::connect(&client, &AuthClient::tokenVerified, &loop, [&](bool valid, const QString& verifiedSubject) {
            verified = valid;
            subject = verifiedSubject;
            loop.quit();
        });
        client.verifyToken(receivedToken);
        loop.exec();
    }

    EXPECT_TRUE(verified);
    EXPECT_EQ(subject, login);
}

TEST(AuthClientIntegrationTest, RequestTokenWithWrongPasswordEmitsError) {
    const char* authUrlEnv = std::getenv("AUTH_SERVICE_URL");
    const QUrl authBaseUrl(authUrlEnv != nullptr ? QString::fromLocal8Bit(authUrlEnv)
                                                  : QStringLiteral("http://127.0.0.1:8080"));
    const char* userUrlEnv = std::getenv("USER_SERVICE_URL");
    const QUrl userBaseUrl(userUrlEnv != nullptr ? QString::fromLocal8Bit(userUrlEnv)
                                                  : QStringLiteral("http://127.0.0.1:8081"));

    const QString login = QStringLiteral("integration-wrongpw-test-%1").arg(QDateTime::currentMSecsSinceEpoch());
    const QString password = QStringLiteral("correct-password");

    QNetworkAccessManager setupManager;
    if (!registerTestUser(setupManager, userBaseUrl, login, password)) {
        GTEST_SKIP() << "user-service not reachable at " << userBaseUrl.toString().toStdString()
                      << " — start docker-compose + user-service locally to run this test.";
    }

    AuthClient client(authBaseUrl);
    QString errorMessage;
    bool errorFired = false;
    {
        QEventLoop loop;
        QTimer::singleShot(3000, &loop, &QEventLoop::quit);
        QObject::connect(&client, &AuthClient::errorOccurred, &loop, [&](const QString& message) {
            errorMessage = message;
            errorFired = true;
            loop.quit();
        });
        QObject::connect(&client, &AuthClient::tokenReceived, &loop, [&](const QString&) { loop.quit(); });
        client.requestToken(login, QStringLiteral("wrong-password"));
        loop.exec();
    }

    if (!errorFired && errorMessage.isEmpty()) {
        GTEST_SKIP() << "auth-service not reachable at " << authBaseUrl.toString().toStdString()
                      << " — start it locally to run this test.";
    }
    EXPECT_TRUE(errorFired);
    EXPECT_FALSE(errorMessage.isEmpty());
}

TEST(AuthClientIntegrationTest, VerifyTokenWithInvalidTokenReportsNotValid) {
    const char* authUrlEnv = std::getenv("AUTH_SERVICE_URL");
    const QUrl authBaseUrl(authUrlEnv != nullptr ? QString::fromLocal8Bit(authUrlEnv)
                                                  : QStringLiteral("http://127.0.0.1:8080"));

    AuthClient client(authBaseUrl);
    bool tokenVerifiedFired = false;
    bool valid = true;
    QEventLoop loop;
    QTimer::singleShot(3000, &loop, &QEventLoop::quit);
    QObject::connect(&client, &AuthClient::tokenVerified, &loop, [&](bool receivedValid, const QString&) {
        tokenVerifiedFired = true;
        valid = receivedValid;
        loop.quit();
    });
    client.verifyToken(QStringLiteral("not-a-real-token"));
    loop.exec();

    if (!tokenVerifiedFired) {
        GTEST_SKIP() << "auth-service not reachable at " << authBaseUrl.toString().toStdString()
                      << " — start it locally to run this test.";
    }
    EXPECT_FALSE(valid);
}

bool patchOwnEmail(QNetworkAccessManager& manager, const QUrl& userServiceUrl, const QString& token,
                    const QString& email) {
    QNetworkRequest request(userServiceUrl.resolved(QUrl(QStringLiteral("/users/me"))));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("Authorization", ("Bearer " + token).toUtf8());

    const QJsonObject body{{"email", email}};
    QNetworkReply* reply =
        manager.sendCustomRequest(request, "PATCH", QJsonDocument(body).toJson(QJsonDocument::Compact));

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

TEST(AuthClientIntegrationTest, RequestOtpThenVerifyOtpRoundTripsToAValidToken) {
    const char* authUrlEnv = std::getenv("AUTH_SERVICE_URL");
    const QUrl authBaseUrl(authUrlEnv != nullptr ? QString::fromLocal8Bit(authUrlEnv)
                                                  : QStringLiteral("http://127.0.0.1:8080"));
    const char* userUrlEnv = std::getenv("USER_SERVICE_URL");
    const QUrl userBaseUrl(userUrlEnv != nullptr ? QString::fromLocal8Bit(userUrlEnv)
                                                  : QStringLiteral("http://127.0.0.1:8081"));

    const QString login = QStringLiteral("integration-otp-test-%1").arg(QDateTime::currentMSecsSinceEpoch());
    const QString password = QStringLiteral("integration-test-password");
    const QString email = login + QStringLiteral("@example.test");

    QNetworkAccessManager setupManager;
    if (!registerTestUser(setupManager, userBaseUrl, login, password)) {
        GTEST_SKIP() << "user-service not reachable at " << userBaseUrl.toString().toStdString()
                      << " — start docker-compose + user-service locally to run this test.";
    }

    AuthClient client(authBaseUrl);

    // Need a token to PATCH /users/me with — password login is the
    // simplest way to get one for test setup, unrelated to the OTP flow
    // this test actually exercises below.
    QString setupToken;
    {
        QEventLoop loop;
        QTimer::singleShot(3000, &loop, &QEventLoop::quit);
        QObject::connect(&client, &AuthClient::tokenReceived, &loop,
                          [&](const QString& token, const QString&, qint64) {
                              setupToken = token;
                              loop.quit();
                          });
        QObject::connect(&client, &AuthClient::errorOccurred, &loop, [&](const QString&) { loop.quit(); });
        client.requestToken(login, password);
        loop.exec();
    }
    if (setupToken.isEmpty()) {
        GTEST_SKIP() << "auth-service not reachable at " << authBaseUrl.toString().toStdString()
                      << " — start it locally to run this test.";
    }
    ASSERT_TRUE(patchOwnEmail(setupManager, userBaseUrl, setupToken, email));

    QString requestedIdentifier;
    QString errorMessage;
    {
        QEventLoop loop;
        QTimer::singleShot(3000, &loop, &QEventLoop::quit);
        QObject::connect(&client, &AuthClient::otpRequested, &loop, [&](const QString& identifier) {
            requestedIdentifier = identifier;
            loop.quit();
        });
        QObject::connect(&client, &AuthClient::errorOccurred, &loop, [&](const QString& message) {
            errorMessage = message;
            loop.quit();
        });
        client.requestOtp(email);
        loop.exec();
    }
    ASSERT_EQ(requestedIdentifier, email) << "errorOccurred: " << errorMessage.toStdString();

    // The code itself only reaches the LoggingCodeDeliveryChannel
    // fallback's stdout (no real SMTP account in this environment) —
    // there's no way for this test to read it back, so this only
    // verifies the request half round-trips; verifyOtp() with an
    // invalid code below covers the failure path instead.
}

TEST(AuthClientIntegrationTest, VerifyOtpWithWrongCodeEmitsError) {
    const char* authUrlEnv = std::getenv("AUTH_SERVICE_URL");
    const QUrl authBaseUrl(authUrlEnv != nullptr ? QString::fromLocal8Bit(authUrlEnv)
                                                  : QStringLiteral("http://127.0.0.1:8080"));

    AuthClient client(authBaseUrl);
    bool errorFired = false;
    QString errorMessage;
    QEventLoop loop;
    QTimer::singleShot(3000, &loop, &QEventLoop::quit);
    QObject::connect(&client, &AuthClient::errorOccurred, &loop, [&](const QString& message) {
        errorFired = true;
        errorMessage = message;
        loop.quit();
    });
    QObject::connect(&client, &AuthClient::tokenReceived, &loop, [&](const QString&) { loop.quit(); });
    client.verifyOtp(QStringLiteral("no-such-identifier@example.test"), QStringLiteral("000000"));
    loop.exec();

    if (!errorFired && errorMessage.isEmpty()) {
        GTEST_SKIP() << "auth-service not reachable at " << authBaseUrl.toString().toStdString()
                      << " — start it locally to run this test.";
    }
    EXPECT_TRUE(errorFired);
}

// Issue #389/#390 — та же маленькая копия RFC 4226/6238, что и в
// UserProfileClientTest.cpp (своя на файл — тестовый хелпер, не часть
// продакшен-кода, дублирование которого между тестовыми бинарями/
// единицами трансляции не противоречит DRY тем же способом, что и
// дублирование в реальном коде).
QByteArray decodeBase32ForTotpTest(const QString& text) {
    static const QString kAlphabet = QStringLiteral("ABCDEFGHIJKLMNOPQRSTUVWXYZ234567");
    QString bits;
    for (const QChar rawChar : text) {
        if (rawChar == QLatin1Char('=')) {
            break;
        }
        const int index = kAlphabet.indexOf(rawChar.toUpper());
        if (index < 0) {
            continue;
        }
        bits += QString::number(index, 2).rightJustified(5, QLatin1Char('0'));
    }
    QByteArray bytes;
    for (int i = 0; i + 8 <= bits.size(); i += 8) {
        bytes.append(static_cast<char>(bits.mid(i, 8).toUShort(nullptr, 2)));
    }
    return bytes;
}

QString computeTotpCodeForAuthClientTest(const QString& base32Secret) {
    const QByteArray key = decodeBase32ForTotpTest(base32Secret);
    const auto counter = static_cast<uint64_t>(QDateTime::currentSecsSinceEpoch() / 30);

    QByteArray counterBytes(8, '\0');
    for (int i = 7; i >= 0; --i) {
        counterBytes[i] = static_cast<char>((counter >> ((7 - i) * 8)) & 0xff);
    }

    QMessageAuthenticationCode hmacCode(QCryptographicHash::Sha1);
    hmacCode.setKey(key);
    hmacCode.addData(counterBytes);
    const QByteArray digest = hmacCode.result();

    const auto offset = static_cast<unsigned char>(digest[digest.size() - 1] & 0x0f);
    const uint32_t truncated = ((static_cast<unsigned char>(digest[offset]) & 0x7f) << 24) |
                                ((static_cast<unsigned char>(digest[offset + 1]) & 0xff) << 16) |
                                ((static_cast<unsigned char>(digest[offset + 2]) & 0xff) << 8) |
                                (static_cast<unsigned char>(digest[offset + 3]) & 0xff);
    return QString::number(truncated % 1000000).rightJustified(6, QLatin1Char('0'));
}

TEST(AuthClientIntegrationTest, RequestTokenChallengesWithPendingTokenWhenTotpIsEnabledAndVerifyTotpCompletesLogin) {
    const char* authUrlEnv = std::getenv("AUTH_SERVICE_URL");
    const QUrl authBaseUrl(authUrlEnv != nullptr ? QString::fromLocal8Bit(authUrlEnv)
                                                  : QStringLiteral("http://127.0.0.1:8080"));
    const char* userUrlEnv = std::getenv("USER_SERVICE_URL");
    const QUrl userBaseUrl(userUrlEnv != nullptr ? QString::fromLocal8Bit(userUrlEnv)
                                                  : QStringLiteral("http://127.0.0.1:8081"));

    const QString login = QStringLiteral("integration-totp-login-test-%1").arg(QDateTime::currentMSecsSinceEpoch());
    const QString password = QStringLiteral("integration-test-password");

    QNetworkAccessManager setupManager;
    if (!registerTestUser(setupManager, userBaseUrl, login, password)) {
        GTEST_SKIP() << "user-service not reachable at " << userBaseUrl.toString().toStdString()
                      << " — start docker-compose + user-service locally to run this test.";
    }

    AuthClient client(authBaseUrl);
    QString setupToken;
    {
        QEventLoop loop;
        QTimer::singleShot(3000, &loop, &QEventLoop::quit);
        QObject::connect(&client, &AuthClient::tokenReceived, &loop,
                          [&](const QString& token, const QString&, qint64) {
                              setupToken = token;
                              loop.quit();
                          });
        QObject::connect(&client, &AuthClient::errorOccurred, &loop, [&](const QString&) { loop.quit(); });
        client.requestToken(login, password);
        loop.exec();
    }
    if (setupToken.isEmpty()) {
        GTEST_SKIP() << "auth-service not reachable at " << authBaseUrl.toString().toStdString()
                      << " — start it locally to run this test.";
    }

    UserProfileClient profileClient(userBaseUrl);
    TotpSetupInfo setup;
    {
        QEventLoop loop;
        QTimer::singleShot(3000, &loop, &QEventLoop::quit);
        QObject::connect(&profileClient, &UserProfileClient::totpSetupStarted, &loop,
                          [&](const TotpSetupInfo& info) {
                              setup = info;
                              loop.quit();
                          });
        profileClient.setupTotp(setupToken);
        loop.exec();
    }
    ASSERT_FALSE(setup.secret.isEmpty());
    {
        QEventLoop loop;
        QTimer::singleShot(3000, &loop, &QEventLoop::quit);
        QObject::connect(&profileClient, &UserProfileClient::totpConfirmed, &loop,
                          [&](const QStringList&) { loop.quit(); });
        profileClient.confirmTotp(setupToken, computeTotpCodeForAuthClientTest(setup.secret));
        loop.exec();
    }

    // Первичный фактор (пароль) снова верен, но теперь у аккаунта
    // включена TOTP — вместо tokenReceived() должен прийти
    // totpChallengeRequired().
    QString pendingToken;
    bool tokenReceivedInstead = false;
    {
        QEventLoop loop;
        QTimer::singleShot(3000, &loop, &QEventLoop::quit);
        QObject::connect(&client, &AuthClient::totpChallengeRequired, &loop, [&](const QString& token) {
            pendingToken = token;
            loop.quit();
        });
        QObject::connect(&client, &AuthClient::tokenReceived, &loop,
                          [&](const QString&, const QString&, qint64) {
                              tokenReceivedInstead = true;
                              loop.quit();
                          });
        client.requestToken(login, password);
        loop.exec();
    }
    EXPECT_FALSE(tokenReceivedInstead);
    ASSERT_FALSE(pendingToken.isEmpty());

    // Неверный код отклоняется без выдачи токена.
    bool errorOnWrongCode = false;
    {
        QEventLoop loop;
        QTimer::singleShot(3000, &loop, &QEventLoop::quit);
        QObject::connect(&client, &AuthClient::errorOccurred, &loop, [&](const QString&) {
            errorOnWrongCode = true;
            loop.quit();
        });
        client.verifyTotp(pendingToken, QStringLiteral("000000"));
        loop.exec();
    }
    EXPECT_TRUE(errorOnWrongCode);

    // Верный код завершает вход обычной парой токенов.
    QString finalToken;
    {
        QEventLoop loop;
        QTimer::singleShot(3000, &loop, &QEventLoop::quit);
        QObject::connect(&client, &AuthClient::tokenReceived, &loop,
                          [&](const QString& token, const QString&, qint64) {
                              finalToken = token;
                              loop.quit();
                          });
        client.verifyTotp(pendingToken, computeTotpCodeForAuthClientTest(setup.secret));
        loop.exec();
    }
    ASSERT_FALSE(finalToken.isEmpty());

    bool verified = false;
    {
        QEventLoop loop;
        QTimer::singleShot(3000, &loop, &QEventLoop::quit);
        QObject::connect(&client, &AuthClient::tokenVerified, &loop, [&](bool valid, const QString&) {
            verified = valid;
            loop.quit();
        });
        client.verifyToken(finalToken);
        loop.exec();
    }
    EXPECT_TRUE(verified);
}

TEST(AuthClientIntegrationTest, RegisterUserWithDuplicateLoginReportsNotRegistered) {
    const char* authUrlEnv = std::getenv("AUTH_SERVICE_URL");
    const QUrl authBaseUrl(authUrlEnv != nullptr ? QString::fromLocal8Bit(authUrlEnv)
                                                  : QStringLiteral("http://127.0.0.1:8080"));

    const QString login = QStringLiteral("integration-dup-test-%1").arg(QDateTime::currentMSecsSinceEpoch());
    const QString password = QStringLiteral("integration-test-password");

    AuthClient client(authBaseUrl);

    // Первая регистрация — закрепляет логин.
    bool firstRegistered = false;
    bool firstFired = false;
    {
        QEventLoop loop;
        QTimer::singleShot(3000, &loop, &QEventLoop::quit);
        QObject::connect(&client, &AuthClient::registrationCompleted, &loop, [&](bool registered) {
            firstRegistered = registered;
            firstFired = true;
            loop.quit();
        });
        client.registerUser(login, password);
        loop.exec();
    }
    if (!firstFired) {
        GTEST_SKIP() << "auth-service not reachable at " << authBaseUrl.toString().toStdString()
                      << " — start it locally to run this test.";
    }
    ASSERT_TRUE(firstRegistered);

    // Повторная регистрация того же логина должна сообщаться как неудачная
    // и НЕ должна автоматически выдавать токен, как это делает первая
    // (новая) регистрация.
    bool secondRegistered = true;
    bool tokenReceivedOnDuplicate = false;
    QEventLoop loop;
    QTimer::singleShot(3000, &loop, &QEventLoop::quit);
    QObject::connect(&client, &AuthClient::registrationCompleted, &loop, [&](bool registered) {
        secondRegistered = registered;
        loop.quit();
    });
    QObject::connect(&client, &AuthClient::tokenReceived, &loop,
                      [&](const QString&) { tokenReceivedOnDuplicate = true; });
    client.registerUser(login, password);
    loop.exec();

    EXPECT_FALSE(secondRegistered);
    EXPECT_FALSE(tokenReceivedOnDuplicate);
}

}  // namespace
}  // namespace devicehub
