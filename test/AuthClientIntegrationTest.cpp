#include "auth/AuthClient.h"

#include <gtest/gtest.h>

#include <QEventLoop>
#include <QTimer>
#include <QUrl>

#include <cstdlib>

// Requires a live auth-service (see services/auth-service) reachable at
// AUTH_SERVICE_URL (default http://127.0.0.1:8080). Skips itself rather
// than failing when the service isn't running — CI doesn't currently
// orchestrate both services together, so this is a manual/local
// end-to-end check, not part of the automated suite's guarantees.

namespace devicehub {
namespace {

TEST(AuthClientIntegrationTest, RequestTokenAndVerifyRoundTrip) {
    const char* urlEnv = std::getenv("AUTH_SERVICE_URL");
    const QUrl baseUrl(urlEnv != nullptr ? QString::fromLocal8Bit(urlEnv) : QStringLiteral("http://127.0.0.1:8080"));

    AuthClient client(baseUrl);

    QString receivedToken;
    QString errorMessage;
    {
        QEventLoop loop;
        QTimer::singleShot(3000, &loop, &QEventLoop::quit);
        QObject::connect(&client, &AuthClient::tokenReceived, &loop, [&](const QString& token) {
            receivedToken = token;
            loop.quit();
        });
        QObject::connect(&client, &AuthClient::errorOccurred, &loop, [&](const QString& message) {
            errorMessage = message;
            loop.quit();
        });
        client.requestToken();
        loop.exec();
    }

    if (receivedToken.isEmpty()) {
        GTEST_SKIP() << "auth-service not reachable at " << baseUrl.toString().toStdString() << " (error: "
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
    EXPECT_EQ(subject, QStringLiteral("devicehub-client"));
}

}  // namespace
}  // namespace devicehub
