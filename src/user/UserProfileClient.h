#pragma once

#include <QNetworkAccessManager>
#include <QObject>
#include <QUrl>

namespace devicehub {

/// Профиль пользователя, как его возвращает REST API user-service —
/// displayName/avatarUrl являются пустыми строками, если не заданы (в
/// отличие от серверной структуры, на клиенте не нужно различать
/// null/не задано: пустое поле везде, где оно отображается, означает
/// «показать логин вместо этого»).
struct UserProfile {
    QString login;
    QString displayName;
    QString avatarUrl;
    /// Публичный ключ X25519 в base64 (issue #136), пуст, если
    /// пользователь ещё не опубликовал его.
    QString publicKey;
};

/**
 * @brief REST-клиент для эндпоинтов профиля user-service:
 *        GET /users/{login}/profile, PATCH /users/me (issue #110).
 *
 * Намеренно отделён от AuthClient (auth-service) и ChatRestClient
 * (chat-service) — user-service владеет данными аккаунта/профиля, это
 * другой сервис со своим собственным базовым URL. Асинхронно через
 * сигналы, как и другие REST-клиенты — никогда не блокирует GUI-поток
 * на сетевом round trip.
 */
class UserProfileClient : public QObject {
    Q_OBJECT

public:
    explicit UserProfileClient(QUrl baseUrl, QObject* parent = nullptr);

    /// Получает профиль @p login.
    void fetchProfile(const QString& token, const QString& login);

    /// Обновляет собственный профиль вызывающего (логин берётся из
    /// @p token на стороне сервера — через этот вызов его никогда
    /// нельзя подделать).
    void updateOwnProfile(const QString& token, const QString& displayName, const QString& avatarUrl);

    /// Публикует публичный ключ X25519 вызывающего (issue #136) —
    /// отдельный метод, а не третий параметр у updateOwnProfile(): это
    /// вызывается из IdentityKeyStore при входе в систему, а это другой
    /// вызывающий код/триггер, чем инициированный пользователем поток
    /// редактирования профиля.
    void publishPublicKey(const QString& token, const QString& publicKey);

signals:
    void profileReceived(const UserProfile& profile);
    void profileUpdated(const UserProfile& profile);
    void errorOccurred(const QString& message);

private:
    QUrl baseUrl_;
    QNetworkAccessManager networkManager_;
};

}  // namespace devicehub
