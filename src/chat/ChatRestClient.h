#pragma once

#include <QList>
#include <QNetworkAccessManager>
#include <QObject>
#include <QUrl>

namespace devicehub {

/// A community or channel as returned by chat-service's REST API.
struct ChatItem {
    qint64 id = 0;
    QString name;
    QString ownerLogin;
};

/// A chat message as returned by chat-service's REST history endpoint —
/// same shape as ChatClient's live messageReceived() fields, plus the
/// id needed to page further back in history (see listMessages()'s
/// beforeId).
struct ChatMessageInfo {
    qint64 id = 0;
    QString author;
    QString body;
    QString sentAt;
};

/**
 * @brief REST client for chat-service's community/channel management:
 *        create/list communities, join, create/list channels.
 *
 * Deliberately separate from ChatClient (WebSocket real-time messaging)
 * — CRUD/management and the live message stream are different
 * responsibilities. Async via signals, like AuthClient — never blocks
 * the GUI thread on a network round trip.
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
    void createChannel(const QString& token, qint64 communityId, const QString& name);
    void listChannels(const QString& token, qint64 communityId);
    void renameChannel(const QString& token, qint64 channelId, const QString& newName);
    void deleteChannel(const QString& token, qint64 channelId);

    /// Fetches a chronological page of up to @p limit messages, ending
    /// just before @p beforeId (or at the newest message if @p beforeId
    /// is negative — the default, for the initial history load).
    void listMessages(const QString& token, qint64 channelId, int limit, qint64 beforeId = -1);

    /// Searches @p channelId's message bodies for a case-insensitive
    /// substring match on @p query (issue #118), newest match first, up
    /// to @p limit results.
    void searchMessages(const QString& token, qint64 channelId, const QString& query, int limit = 20);

signals:
    void communityCreated(qint64 id, const QString& name);
    void communitiesListed(const QList<ChatItem>& communities);
    void communityRenamed(qint64 id, const QString& newName);
    void communityDeleted(qint64 id);
    void communityJoined(qint64 communityId);
    void channelCreated(qint64 id, const QString& name);
    void channelsListed(const QList<ChatItem>& channels);
    void channelRenamed(qint64 id, const QString& newName);
    void channelDeleted(qint64 id);
    /// Reply to listMessages() — @p messages is chronological (oldest
    /// to newest), possibly empty if there's no more history.
    void messagesListed(qint64 channelId, const QList<ChatMessageInfo>& messages);

    /// Reply to searchMessages() — @p matches is newest-first, possibly
    /// empty.
    void messagesFound(qint64 channelId, const QString& query, const QList<ChatMessageInfo>& matches);

    void errorOccurred(const QString& message);

private:
    QUrl baseUrl_;
    QNetworkAccessManager networkManager_;
};

}  // namespace devicehub
