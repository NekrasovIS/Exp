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
    void errorOccurred(const QString& message);

private:
    QUrl baseUrl_;
    QNetworkAccessManager networkManager_;
};

}  // namespace devicehub
