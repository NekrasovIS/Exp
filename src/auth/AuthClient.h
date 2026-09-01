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
 *
 * Login/register/refresh (issue #105) all report their access token the
 * same way, via tokenReceived(token, refreshToken, expiresAt) — a
 * uniform signal means MainWindow doesn't need separate handling for
 * "freshly logged in" vs. "silently refreshed". refreshAccessToken()
 * doesn't get back (or need) a new refresh token — refresh tokens don't
 * rotate on this service, see TokenService's doc comment — so it
 * re-emits whatever refresh token was passed in unchanged, alongside
 * the freshly issued access token/expiry.
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

    /// Exchanges @p refreshToken for a fresh access token via
    /// POST {baseUrl}/auth/refresh — lets the caller stay signed in
    /// past the access token's short TTL without re-entering
    /// credentials. Reports the result via tokenReceived(), same as
    /// requestToken()/registerUser().
    void refreshAccessToken(const QString& refreshToken);

    /// Запрашивает одноразовый код для @p identifier (login или email,
    /// issue #156) через POST {baseUrl}/auth/otp/request — auth-service
    /// всегда отвечает успехом независимо от того, существует ли такой
    /// аккаунт, поэтому единственное, что можно узнать из результата —
    /// прошёл ли сам сетевой запрос (otpRequested()/errorOccurred()).
    void requestOtp(const QString& identifier);

    /// Проверяет @p code для @p identifier через POST
    /// {baseUrl}/auth/otp/verify — при совпадении выдаёт токен, отдаёт
    /// его через tokenReceived(), как и остальные способы входа.
    void verifyOtp(const QString& identifier, const QString& code);

signals:
    /// Emitted when a new access token was issued successfully — by
    /// requestToken(), registerUser() (auto-login), or
    /// refreshAccessToken(). @p expiresAt is Unix seconds (UTC).
    void tokenReceived(const QString& token, const QString& refreshToken, qint64 expiresAt);

    /// Emitted with the result of a verifyToken() call.
    void tokenVerified(bool valid, const QString& subject);

    /// Emitted with the result of a registerUser() call. On success,
    /// tokenReceived() also fires with the auto-issued token.
    void registrationCompleted(bool registered);

    /// Emitted on any network or protocol error for any of the calls
    /// above.
    void errorOccurred(const QString& message);

    /// Emitted when requestOtp() round-tripped successfully — echoes
    /// back @p identifier so the caller (LoginWindow) doesn't need to
    /// track it separately.
    void otpRequested(const QString& identifier);

private:
    QUrl baseUrl_;
    QNetworkAccessManager networkManager_;
};

}  // namespace devicehub
