#pragma once

#include <QByteArray>
#include <QList>
#include <QNetworkAccessManager>
#include <QObject>
#include <QStringList>
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
    /// Issue #156: пуст, пока пользователь не задаст — нужен, прежде
    /// чем можно будет использовать вход по одноразовому коду, иначе не
    /// используется на клиенте.
    QString email;
    /// Issue #174: empty until the user links their Telegram — an
    /// alternative one-time-code delivery channel, preferred over email
    /// server-side when both are set.
    QString telegramChatId;
};

/// Поля, которые может изменить updateOwnProfile() — сгруппированы
/// согласно правилу CLAUDE.md "предпочитать меньшее количество
/// аргументов функций" (displayName/avatarUrl/email/telegramChatId —
/// четыре однотипных QString подряд, легко перепутать местами при
/// вызове), вместо того чтобы дальше расширять список параметров самого
/// updateOwnProfile().
struct ProfileEdits {
    QString displayName;
    QString avatarUrl;
    QString email;
    QString telegramChatId;
};

/// Входящая заявка в друзья (issue #187, Фаза 1), как её возвращает
/// GET /friends/requests.
struct FriendRequestInfo {
    qint64 id = 0;
    QString requesterLogin;
    QString createdAt;
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
    void updateOwnProfile(const QString& token, const ProfileEdits& edits);

    /// Публикует публичный ключ X25519 вызывающего (issue #136) —
    /// отдельный метод, а не третий параметр у updateOwnProfile(): это
    /// вызывается из IdentityKeyStore при входе в систему, а это другой
    /// вызывающий код/триггер, чем инициированный пользователем поток
    /// редактирования профиля.
    void publishPublicKey(const QString& token, const QString& publicKey);

    /// Отправляет заявку в друзья @p recipientLogin (issue #187, Фаза
    /// 1) — вызывает friendRequestSent() с @p status "sent" или
    /// "accepted" (у получателя уже была встречная pending-заявка).
    void sendFriendRequest(const QString& token, const QString& recipientLogin);
    /// Входящие pending-заявки вызывающего.
    void listIncomingFriendRequests(const QString& token);
    void acceptFriendRequest(const QString& token, qint64 requestId);
    void declineFriendRequest(const QString& token, qint64 requestId);
    void listFriends(const QString& token);
    /// Расфрендить — работает в любую сторону пары.
    void removeFriend(const QString& token, const QString& login);

    /// Загружает изображение аватара вызывающего (issue #384/#442) —
    /// тот же base64+content-type контракт, что и у веб-клиента
    /// (packages/core's UserServiceClient.uploadAvatar()). Сервер сам
    /// обновляет avatar_url профиля как побочный эффект — этот вызов не
    /// нужно сопровождать отдельным updateOwnProfile().
    void uploadAvatar(const QString& token, const QString& contentType, const QByteArray& data);

    /// Скачивает сохранённый аватар @p login (issue #384/#442) — без
    /// токена: сам эндпоинт публичный, тот же доступ, что и у
    /// avatar_url в GET .../profile.
    void fetchAvatar(const QString& login);

signals:
    void profileReceived(const UserProfile& profile);
    void profileUpdated(const UserProfile& profile);

    /// Успешная uploadAvatar() — @p avatarUrl тот же
    /// "/users/<login>/avatar", что сервер только что записал в
    /// avatar_url профиля.
    void avatarUploaded(const QString& avatarUrl);
    /// Успешная fetchAvatar() — @p data не декодирован (уже сырые
    /// байты изображения, не base64 — сервер отдаёт их напрямую, см.
    /// docs/services/user-service.md), @p contentType из заголовка
    /// ответа.
    void avatarFetched(const QString& login, const QByteArray& data, const QString& contentType);
    /// fetchAvatar() не нашла аватар (404) или сеть отказала —
    /// намеренно отдельный сигнал, а не errorOccurred(): у пользователя
    /// без загруженного аватара это ожидаемый, частый случай (не
    /// каждая ошибка), реагировать на который тостом было бы шумно.
    void avatarFetchFailed(const QString& login);

    void friendRequestSent(const QString& recipientLogin, const QString& status);
    void incomingFriendRequestsListed(const QList<FriendRequestInfo>& requests);
    void friendRequestAccepted(qint64 requestId);
    void friendRequestDeclined(qint64 requestId);
    void friendsListed(const QStringList& logins);
    void friendRemoved(const QString& login);

    void errorOccurred(const QString& message);

private:
    QUrl baseUrl_;
    QNetworkAccessManager networkManager_;
};

}  // namespace devicehub
