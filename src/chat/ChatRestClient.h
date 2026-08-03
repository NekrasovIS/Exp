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
    void joinCommunity(const QString& token, qint64 communityId);
    void createChannel(const QString& token, qint64 communityId, const QString& name);
    void listChannels(const QString& token, qint64 communityId);

signals:
    void communityCreated(qint64 id, const QString& name);
    void communitiesListed(const QList<ChatItem>& communities);
    void communityJoined(qint64 communityId);
    void channelCreated(qint64 id, const QString& name);
    void channelsListed(const QList<ChatItem>& channels);
    void errorOccurred(const QString& message);

private:
    QUrl baseUrl_;
    QNetworkAccessManager networkManager_;
};

}  // namespace devicehub
