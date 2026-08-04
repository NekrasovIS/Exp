#pragma once

#include <QNetworkAccessManager>
#include <QObject>
#include <QUrl>

namespace devicehub {

/**
 * @brief Talks to auth-service over HTTP: requests a token, then can ask
 *        the same service to verify one.
 *
 * Async via signals, like the classes in devices/ — never blocks the GUI
 * thread on a network round trip.
 */
class AuthClient : public QObject {
    Q_OBJECT

public:
    explicit AuthClient(QUrl baseUrl, QObject* parent = nullptr);

    /// Requests a new token for (@p login, @p password) from
    /// POST {baseUrl}/auth/token — auth-service checks these against
    /// user-service before issuing anything.
    void requestToken(const QString& login, const QString& password);

    /// Asks the service to verify @p token via POST {baseUrl}/auth/verify.
    void verifyToken(const QString& token);

    /// Registers a new account via POST {baseUrl}/auth/register —
    /// auth-service forwards to user-service and, on success, issues a
    /// token immediately (auto-login), reported via tokenReceived().
    void registerUser(const QString& login, const QString& password);

signals:
    /// Emitted when a new token was issued successfully.
    void tokenReceived(const QString& token);

    /// Emitted with the result of a verifyToken() call.
    void tokenVerified(bool valid, const QString& subject);

    /// Emitted with the result of a registerUser() call. On success,
    /// tokenReceived() also fires with the auto-issued token.
    void registrationCompleted(bool registered);

    /// Emitted on any network or protocol error for any of the calls
    /// above.
    void errorOccurred(const QString& message);

private:
    QUrl baseUrl_;
    QNetworkAccessManager networkManager_;
};

}  // namespace devicehub
