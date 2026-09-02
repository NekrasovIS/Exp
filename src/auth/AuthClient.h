#pragma once

#include <QNetworkAccessManager>
#include <QObject>
#include <QUrl>

namespace devicehub {

/**
 * @brief Общается с auth-service по HTTP: запрашивает токен, а затем может
 *        попросить тот же сервис проверить его.
 *
 * Асинхронно через сигналы, как и классы в devices/ — никогда не блокирует
 * GUI-поток на сетевом round trip.
 *
 * Login/register/refresh (issue #105) — все отдают свой access-токен
 * одинаково, через tokenReceived(token, refreshToken, expiresAt) —
 * единообразный сигнал избавляет MainWindow от отдельной обработки
 * «только что залогинился» против «тихо обновил токен». refreshAccessToken()
 * не получает обратно (и ему не нужен) новый refresh-токен — refresh-токены
 * на этом сервисе не ротируются, см. doc-комментарий TokenService — поэтому
 * он повторно испускает без изменений тот refresh-токен, что был передан на
 * вход, вместе со свежевыпущенным access-токеном/сроком действия.
 */
class AuthClient : public QObject {
    Q_OBJECT

public:
    explicit AuthClient(QUrl baseUrl, QObject* parent = nullptr);

    /// Запрашивает новый токен для (@p login, @p password) через
    /// POST {baseUrl}/auth/token — auth-service сверяет их с user-service
    /// перед тем как что-либо выпустить.
    void requestToken(const QString& login, const QString& password);

    /// Просит сервис проверить @p token через POST {baseUrl}/auth/verify.
    void verifyToken(const QString& token);

    /// Регистрирует новый аккаунт через POST {baseUrl}/auth/register —
    /// auth-service перенаправляет запрос в user-service и при успехе сразу
    /// выпускает токен (auto-login), о чём сообщается через tokenReceived().
    void registerUser(const QString& login, const QString& password);

    /// Обменивает @p refreshToken на свежий access-токен через
    /// POST {baseUrl}/auth/refresh — позволяет вызывающему коду оставаться
    /// авторизованным дольше короткого TTL access-токена без повторного
    /// ввода учётных данных. Сообщает результат через tokenReceived(), как
    /// requestToken()/registerUser().
    void refreshAccessToken(const QString& refreshToken);

signals:
    /// Испускается при успешном выпуске нового access-токена — из
    /// requestToken(), registerUser() (auto-login) или
    /// refreshAccessToken(). @p expiresAt — Unix-секунды (UTC).
    void tokenReceived(const QString& token, const QString& refreshToken, qint64 expiresAt);

    /// Испускается с результатом вызова verifyToken().
    void tokenVerified(bool valid, const QString& subject);

    /// Испускается с результатом вызова registerUser(). При успехе также
    /// срабатывает tokenReceived() с автоматически выпущенным токеном.
    void registrationCompleted(bool registered);

    /// Испускается при любой сетевой или протокольной ошибке для любого из
    /// вызовов выше.
    void errorOccurred(const QString& message);

private:
    QUrl baseUrl_;
    QNetworkAccessManager networkManager_;
};

}  // namespace devicehub
