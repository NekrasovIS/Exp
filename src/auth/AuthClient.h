#pragma once

#include <QNetworkAccessManager>
#include <QObject>
#include <QUrl>

namespace devicehub {

/**
 * @brief Talks to auth-service over HTTP: requests a token, then can ask
 *        the same service to verify one.
 *
 * Async via signals, like the devices/* classes — never blocks the GUI
 * thread on a network round trip.
 */
class AuthClient : public QObject {
    Q_OBJECT

public:
    explicit AuthClient(QUrl baseUrl, QObject* parent = nullptr);

    /// Requests a new token for @p subject from POST {baseUrl}/auth/token.
    void requestToken(const QString& subject = QStringLiteral("devicehub-client"));

    /// Asks the service to verify @p token via POST {baseUrl}/auth/verify.
    void verifyToken(const QString& token);

signals:
    /// Emitted when a new token was issued successfully.
    void tokenReceived(const QString& token);

    /// Emitted with the result of a verifyToken() call.
    void tokenVerified(bool valid, const QString& subject);

    /// Emitted on any network or protocol error for either call.
    void errorOccurred(const QString& message);

private:
    QUrl baseUrl_;
    QNetworkAccessManager networkManager_;
};

}  // namespace devicehub
