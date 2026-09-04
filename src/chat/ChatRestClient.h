#pragma once

#include <QList>
#include <QNetworkAccessManager>
#include <QObject>
#include <QStringList>
#include <QUrl>

namespace devicehub {

/// Сообщество или канал, как их возвращает REST API chat-service.
/// isEncrypted не имеет смысла (всегда false) для сообщества — только
/// каналы могут быть зашифрованы (issue #138). inviteCode (issue #186)
/// заполнен только для собственных сообществ (listCommunities(), см. её
/// doc-комментарий) — сервер не отдаёт его в местах, где вызывающая
/// сторона ещё не подтвердила членство, и никогда для каналов.
struct ChatItem {
    qint64 id = 0;
    QString name;
    QString ownerLogin;
    bool isEncrypted = false;
    QString inviteCode;
};

/// Сообщение чата, как его возвращает REST-эндпоинт истории
/// chat-service — та же форма, что и поля messageReceived() у ChatClient
/// в реальном времени, плюс id, нужный для постраничной прокрутки
/// истории дальше назад (см. beforeId у listMessages()). @p attachmentId
/// равен -1, а @p attachmentFilename пуст, когда у сообщения нет
/// вложения (issue #116).
struct ChatMessageInfo {
    qint64 id = 0;
    QString author;
    QString body;
    QString sentAt;
    qint64 attachmentId = -1;
    QString attachmentFilename;
};

/**
 * @brief REST-клиент для управления сообществами/каналами chat-service:
 *        создание/список сообществ, вступление, создание/список каналов.
 *
 * Намеренно отделён от ChatClient (обмен сообщениями в реальном времени
 * через WebSocket) — CRUD/управление и живой поток сообщений — разные
 * зоны ответственности. Асинхронно через сигналы, как AuthClient —
 * никогда не блокирует GUI-поток на сетевом round trip.
 */
class ChatRestClient : public QObject {
    Q_OBJECT

public:
    explicit ChatRestClient(QUrl baseUrl, QObject* parent = nullptr);

    void createCommunity(const QString& token, const QString& name);
    /// Issue #186: только сообщества, в которых вызывающая сторона уже
    /// состоит (GET /communities/mine на стороне chat-service, не
    /// GET /communities) — подключение к новому сообществу теперь идёт
    /// по коду приглашения (joinCommunityByCode()), а не выбором из
    /// общего списка всех существующих.
    void listCommunities(const QString& token);
    void renameCommunity(const QString& token, qint64 communityId, const QString& newName);
    void deleteCommunity(const QString& token, qint64 communityId);
    void joinCommunity(const QString& token, qint64 communityId);
    /// Присоединяет к сообществу по его коду приглашения (issue #186), а
    /// не по известному id — вызывает joinedCommunityByCode() при успехе
    /// (сообщает и id, и name сразу, поскольку до этого момента
    /// вызывающая сторона не знала ни того, ни другого) или
    /// errorOccurred() с "invalid invite code", если такого кода нет.
    void joinCommunityByCode(const QString& token, const QString& code);
    /// Только для владельца @p communityId (issue #186) — прежний код
    /// сразу перестаёт работать. Вызывает inviteCodeRegenerated() при
    /// успехе.
    void regenerateInviteCode(const QString& token, qint64 communityId);
    /// @p isEncrypted (issue #138) фиксируется при создании — почему
    /// изменить его впоследствии невозможно, см. doc-комментарий
    /// Channel::isEncrypted на стороне chat-service.
    void createChannel(const QString& token, qint64 communityId, const QString& name, bool isEncrypted = false);
    void listChannels(const QString& token, qint64 communityId);

    /// Логины всех текущих участников @p communityId (issue #138) —
    /// нужны, чтобы знать, для кого должен быть запечатан ключ
    /// зашифрованного канала.
    void listMembers(const QString& token, qint64 communityId);

    /// Сохраняет @p wrappedKey (уже запечатанный на стороне клиента для
    /// публичного ключа @p memberLogin — см. ChannelCrypto) как их
    /// копию симметричного ключа @p channelId. Вызывать это может
    /// только владелец канала/сообщества или модератор сообщества
    /// (проверяется на сервере).
    void setChannelKey(const QString& token, qint64 channelId, const QString& memberLogin, const QString& wrappedKey);

    /// Получает собственную запечатанную копию ключа @p channelId для
    /// вызывающего. myChannelKeyNotFound() срабатывает (не
    /// errorOccurred()), когда ключ ещё не был задан — это ожидаемое
    /// состояние (например, вступил после создания), а не сбой.
    void fetchMyChannelKey(const QString& token, qint64 channelId);
    void renameChannel(const QString& token, qint64 channelId, const QString& newName);
    void deleteChannel(const QString& token, qint64 channelId);

    /// Только для владельца на стороне сервера (issue #114) —
    /// @p targetLogin не обязан уже быть участником, повышение неявно
    /// добавляет его в состав.
    void promoteModerator(const QString& token, qint64 communityId, const QString& targetLogin);
    /// Только для владельца. Идемпотентно — понижение того, кто не
    /// модератор, тоже завершается успехом.
    void demoteModerator(const QString& token, qint64 communityId, const QString& targetLogin);
    void listModerators(const QString& token, qint64 communityId);

    /// Получает хронологическую страницу до @p limit сообщений,
    /// заканчивающуюся непосредственно перед @p beforeId (или на самом
    /// новом сообщении, если @p beforeId отрицателен — значение по
    /// умолчанию, для изначальной загрузки истории).
    void listMessages(const QString& token, qint64 channelId, int limit, qint64 beforeId = -1);

    /// Загружает @p data (сырые байты, на проводе в виде base64) как
    /// новое вложение в @p channelId (issue #116); вызывает
    /// attachmentUploaded() с id, который затем передаётся в
    /// ChatClient::sendMessage().
    void uploadAttachment(const QString& token, qint64 channelId, const QString& filename,
                           const QString& contentType, const QByteArray& data);

    /// Скачивает сырые байты вложения @p attachmentId; вызывает
    /// attachmentDownloaded().
    void downloadAttachment(const QString& token, qint64 attachmentId);

    /// Ищет в телах сообщений @p channelId регистронезависимое
    /// совпадение подстроки с @p query (issue #118), самое новое
    /// совпадение первым, до @p limit результатов.
    void searchMessages(const QString& token, qint64 channelId, const QString& query, int limit = 20);

signals:
    /// @p inviteCode (issue #186) — создатель сразу видит код, который
    /// предстоит раздавать, без отдельного запроса.
    void communityCreated(qint64 id, const QString& name, const QString& inviteCode);
    void communitiesListed(const QList<ChatItem>& communities);
    void communityRenamed(qint64 id, const QString& newName);
    void communityDeleted(qint64 id);
    void communityJoined(qint64 communityId);
    /// Ответ на joinCommunityByCode() (issue #186) — @p id/@p name, а не
    /// только id, как у communityJoined(): до этого вызова вызывающая
    /// сторона не знала ни того, ни другого.
    void joinedCommunityByCode(qint64 id, const QString& name);
    /// Ответ на regenerateInviteCode() (issue #186).
    void inviteCodeRegenerated(qint64 communityId, const QString& inviteCode);
    void channelCreated(qint64 id, const QString& name, bool isEncrypted);
    void channelsListed(const QList<ChatItem>& channels);
    void channelRenamed(qint64 id, const QString& newName);
    void channelDeleted(qint64 id);
    void membersListed(qint64 communityId, const QStringList& logins);
    void channelKeySet(qint64 channelId, const QString& memberLogin);
    void myChannelKeyFetched(qint64 channelId, const QString& wrappedKey);
    void myChannelKeyNotFound(qint64 channelId);
    void moderatorPromoted(qint64 communityId, const QString& login);
    void moderatorDemoted(qint64 communityId, const QString& login);
    void moderatorsListed(qint64 communityId, const QStringList& logins);
    /// Ответ на listMessages() — @p messages в хронологическом порядке
    /// (от самых старых к самым новым), возможно пуст, если больше нет
    /// истории.
    void messagesListed(qint64 channelId, const QList<ChatMessageInfo>& messages);

    /// Ответ на uploadAttachment().
    void attachmentUploaded(qint64 id, const QString& filename);

    /// Ответ на downloadAttachment() — @p data — сырые (декодированные)
    /// байты вложения.
    void attachmentDownloaded(qint64 attachmentId, const QByteArray& data);

    /// Ответ на searchMessages() — @p matches отсортирован от самых
    /// новых, возможно пуст.
    void messagesFound(qint64 channelId, const QString& query, const QList<ChatMessageInfo>& matches);

    void errorOccurred(const QString& message);

private:
    QUrl baseUrl_;
    QNetworkAccessManager networkManager_;
};

}  // namespace devicehub
