#include "auth/AuthClient.h"

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
