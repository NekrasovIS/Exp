#pragma once

#include <QList>
#include <QNetworkAccessManager>
#include <QObject>
#include <QStringList>
#include <QUrl>

namespace devicehub {

/// Сообщество или канал, как их возвращает REST API chat-service.
/// isEncrypted не имеет смысла (всегда false) для сообщества — только
/// каналы могут быть зашифрованы (issue #138).
struct ChatItem {
    qint64 id = 0;
    QString name;
    QString ownerLogin;
    bool isEncrypted = false;
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

/// Диалог личных сообщений, как его возвращает REST API chat-service
/// (issue #187, Фаза 2).
struct DirectMessageThreadInfo {
    qint64 id = 0;
    QString otherLogin;
    QString createdAt;
};

/// Сообщение в диалоге личных сообщений — без вложений/edited_at в
/// этой фазе, в отличие от ChatMessageInfo (см. doc-комментарий
/// direct_messages в init.sql на стороне chat-service).
struct DirectMessageInfo {
    qint64 id = 0;
    QString author;
    QString body;
    QString sentAt;
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
    void listCommunities(const QString& token);
    void renameCommunity(const QString& token, qint64 communityId, const QString& newName);
    void deleteCommunity(const QString& token, qint64 communityId);
    void joinCommunity(const QString& token, qint64 communityId);
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

    /// Открывает диалог с @p recipientLogin (issue #187, Фаза 2) —
    /// идемпотентно, тот же id при повторном вызове; вызывает
    /// dmThreadOpened(), либо errorOccurred() (403 "can only message
    /// friends", если получатель не друг, или 400 при попытке написать
    /// самому себе).
    void openDmThread(const QString& token, const QString& recipientLogin);

    void listDmThreads(const QString& token);

    void sendDirectMessage(const QString& token, qint64 threadId, const QString& body);

    /// Тот же постраничный контракт, что и у listMessages().
    void listDirectMessages(const QString& token, qint64 threadId, int limit, qint64 beforeId = -1);

signals:
    void communityCreated(qint64 id, const QString& name);
    void communitiesListed(const QList<ChatItem>& communities);
    void communityRenamed(qint64 id, const QString& newName);
    void communityDeleted(qint64 id);
    void communityJoined(qint64 communityId);
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

    /// Ответ на openDmThread() — @p otherLogin переносится из вызова
    /// (сервер отвечает только id, не логин собеседника — вызывающая
    /// сторона его и так уже знает).
    void dmThreadOpened(qint64 id, const QString& otherLogin);
    void dmThreadsListed(const QList<DirectMessageThreadInfo>& threads);
    void directMessageSent(qint64 threadId, const DirectMessageInfo& message);
    /// Ответ на listDirectMessages() — тот же хронологический порядок,
    /// что и у messagesListed().
    void directMessagesListed(qint64 threadId, const QList<DirectMessageInfo>& messages);

    void errorOccurred(const QString& message);

private:
    QUrl baseUrl_;
    QNetworkAccessManager networkManager_;
};

}  // namespace devicehub
