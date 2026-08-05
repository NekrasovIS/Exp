#pragma once

#include <QJsonObject>
#include <QObject>
#include <QStringList>
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
 *
 * Also mirrors chat-service's group-voice-call signaling relay (issue
 * #46): joinCall()/leaveCall() send `{"call_join"}`/`{"call_leave"}`,
 * sendCallSignal() sends `{"call_signal": {"to", "payload"}}` (an
 * opaque SDP/ICE payload this class never inspects), and the matching
 * callRosterReceived/callPeerJoined/callPeerLeft/callSignalReceived
 * signals fire for the corresponding server frames. Only valid once
 * subscribed() has fired.
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

    /// Joins the voice call for the subscribed channel; triggers a
    /// callRosterReceived() reply.
    void joinCall();

    /// Leaves the voice call for the subscribed channel.
    void leaveCall();

    /// Relays an opaque signaling @p payload (SDP offer/answer or ICE
    /// candidate) to call participant @p to.
    void sendCallSignal(const QString& to, const QJsonObject& payload);

signals:
    /// Emitted once chat-service confirms the subscription.
    void subscribed(qint64 channelId);

    /// Emitted for every message broadcast on the subscribed channel.
    void messageReceived(const QString& author, const QString& body, const QString& sentAt);

    void errorOccurred(const QString& message);

    /// Reply to joinCall(): existing call participants, not including self.
    void callRosterReceived(const QStringList& participants);

    /// Another participant joined the call.
    void callPeerJoined(const QString& login);

    /// A participant left the call (explicit leaveCall() or disconnect).
    void callPeerLeft(const QString& login);

    /// Signaling payload relayed from another call participant.
    void callSignalReceived(const QString& from, const QJsonObject& payload);

private:
    void onConnected();
    void onTextMessageReceived(const QString& message);

    QUrl webSocketUrl_;
    QWebSocket webSocket_;
    QString pendingToken_;
    qint64 pendingChannelId_ = 0;
};

}  // namespace devicehub
