#pragma once

#include <QObject>
#include <QUrl>
#include <QWebSocket>

namespace devicehub {

/**
 * @brief Real-time channel connection to chat-service over WebSocket.
 *
 * Protocol: after connectToChannel(), the first frame sent is
 * `{"token", "channel_id"}`; once chat-service confirms with
 * `{"subscribed": true}`, sendMessage() posts `{"body"}` frames and
 * incoming broadcasts (from any subscriber, including this client) fire
 * messageReceived(). Async via signals, like AuthClient and the
 * classes in devices/ — never blocks the GUI thread.
 */
class ChatClient : public QObject {
    Q_OBJECT

public:
    explicit ChatClient(QUrl webSocketUrl, QObject* parent = nullptr);

    /// Opens the connection and subscribes to @p channelId using @p token.
    void connectToChannel(const QString& token, qint64 channelId);

    /// Posts @p body to the channel this client is subscribed to.
    void sendMessage(const QString& body);

    void disconnectFromChannel();

signals:
    /// Emitted once chat-service confirms the subscription.
    void subscribed(qint64 channelId);

    /// Emitted for every message broadcast on the subscribed channel.
    void messageReceived(const QString& author, const QString& body, const QString& sentAt);

    void errorOccurred(const QString& message);

private:
    void onConnected();
    void onTextMessageReceived(const QString& message);

    QUrl webSocketUrl_;
    QWebSocket webSocket_;
    QString pendingToken_;
    qint64 pendingChannelId_ = 0;
};

}  // namespace devicehub
