#pragma once

#include <QList>
#include <QNetworkAccessManager>
#include <QObject>
#include <QStringList>
#include <QUrl>

namespace devicehub {

/// A community or channel as returned by chat-service's REST API.
/// isEncrypted is meaningless (always false) for a community — only
/// channels can be encrypted (issue #138).
struct ChatItem {
    qint64 id = 0;
    QString name;
    QString ownerLogin;
    bool isEncrypted = false;
};

/// A chat message as returned by chat-service's REST history endpoint —
/// same shape as ChatClient's live messageReceived() fields, plus the
/// id needed to page further back in history (see listMessages()'s
/// beforeId). @p attachmentId is -1 and @p attachmentFilename empty when
/// the message has no attachment (issue #116).
struct ChatMessageInfo {
    qint64 id = 0;
    QString author;
    QString body;
    QString sentAt;
    qint64 attachmentId = -1;
    QString attachmentFilename;
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
    /// @p isEncrypted (issue #138) is fixed at creation — see
    /// Channel::isEncrypted's doc comment on the chat-service side for
    /// why there's no way to change it afterwards.
    void createChannel(const QString& token, qint64 communityId, const QString& name, bool isEncrypted = false);
    void listChannels(const QString& token, qint64 communityId);

    /// Every current member's login for @p communityId (issue #138) —
    /// needed to know who an encrypted channel's key must be wrapped for.
    void listMembers(const QString& token, qint64 communityId);

    /// Stores @p wrappedKey (already sealed client-side for
    /// @p memberLogin's public key — see ChannelCrypto) as their copy of
    /// @p channelId's symmetric key. Only the channel/community owner or
    /// a community moderator may call this (server-enforced).
    void setChannelKey(const QString& token, qint64 channelId, const QString& memberLogin, const QString& wrappedKey);

    /// Fetches the caller's own wrapped copy of @p channelId's key.
    /// myChannelKeyNotFound() fires (not errorOccurred()) when none has
    /// been set yet — an expected state (e.g. joined after creation),
    /// not a failure.
    void fetchMyChannelKey(const QString& token, qint64 channelId);
    void renameChannel(const QString& token, qint64 channelId, const QString& newName);
    void deleteChannel(const QString& token, qint64 channelId);

    /// Owner-only server-side (issue #114) — @p targetLogin doesn't need
    /// to already be a member, promoting joins them implicitly.
    void promoteModerator(const QString& token, qint64 communityId, const QString& targetLogin);
    /// Owner-only. Idempotent — demoting a non-moderator succeeds too.
    void demoteModerator(const QString& token, qint64 communityId, const QString& targetLogin);
    void listModerators(const QString& token, qint64 communityId);

    /// Fetches a chronological page of up to @p limit messages, ending
    /// just before @p beforeId (or at the newest message if @p beforeId
    /// is negative — the default, for the initial history load).
    void listMessages(const QString& token, qint64 channelId, int limit, qint64 beforeId = -1);

    /// Uploads @p data (raw bytes, base64-encoded on the wire) as a new
    /// attachment on @p channelId (issue #116); triggers attachmentUploaded()
    /// with the id to then pass to ChatClient::sendMessage().
    void uploadAttachment(const QString& token, qint64 channelId, const QString& filename,
                           const QString& contentType, const QByteArray& data);

    /// Downloads the raw bytes of attachment @p attachmentId; triggers
    /// attachmentDownloaded().
    void downloadAttachment(const QString& token, qint64 attachmentId);

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
    /// Reply to listMessages() — @p messages is chronological (oldest
    /// to newest), possibly empty if there's no more history.
    void messagesListed(qint64 channelId, const QList<ChatMessageInfo>& messages);

    /// Reply to uploadAttachment().
    void attachmentUploaded(qint64 id, const QString& filename);

    /// Reply to downloadAttachment() — @p data is the raw (decoded)
    /// attachment bytes.
    void attachmentDownloaded(qint64 attachmentId, const QByteArray& data);

    /// Reply to searchMessages() — @p matches is newest-first, possibly
    /// empty.
    void messagesFound(qint64 channelId, const QString& query, const QList<ChatMessageInfo>& matches);

    void errorOccurred(const QString& message);

private:
    QUrl baseUrl_;
    QNetworkAccessManager networkManager_;
};

}  // namespace devicehub
