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
 *
 * sendTyping() (issue #96) sends `{"typing": true}`; userTyping() fires
 * for the matching `{"user_typing": "<login>"}` broadcast from another
 * subscriber (chat-service never echoes this back to its sender).
 * Ephemeral like call presence — no "stopped typing" message exists,
 * ChatView times its own indicator out instead.
 *
 * sendEditMessage()/sendDeleteMessage() (issue #107) send
 * `{"edit_message": {"id", "body"}}`/`{"delete_message": {"id"}}` —
 * chat-service only allows a message's own author to edit/delete it
 * (never the channel/community owner acting on someone else's
 * message), reported back as an errorOccurred() if rejected. On
 * success every subscriber (including the editor/deleter) gets
 * messageEdited()/messageDeleted(), the same "broadcast to everyone,
 * don't locally optimistic-update" pattern messageReceived() already
 * uses for new messages.
 *
 * sendMessage()'s optional @p attachmentId (issue #116) references a
 * file already uploaded via ChatRestClient::uploadAttachment() — this
 * class never touches attachment bytes itself, only the id/filename
 * that ride along in messageReceived().
 */
class ChatClient : public QObject {
    Q_OBJECT

public:
    explicit ChatClient(QUrl webSocketUrl, QObject* parent = nullptr);

    /// Opens the connection and subscribes to @p channelId using @p token.
    void connectToChannel(const QString& token, qint64 channelId);

    /// Posts @p body to the channel this client is subscribed to,
    /// optionally referencing an already-uploaded @p attachmentId
    /// (issue #116) — -1 (the default) means no attachment.
    void sendMessage(const QString& body, qint64 attachmentId = -1);

    void disconnectFromChannel();

    /// Joins the voice call for the subscribed channel; triggers a
    /// callRosterReceived() reply.
    void joinCall();

    /// Leaves the voice call for the subscribed channel.
    void leaveCall();

    /// Relays an opaque signaling @p payload (SDP offer/answer or ICE
    /// candidate) to call participant @p to.
    void sendCallSignal(const QString& to, const QJsonObject& payload);

    /// Tells chat-service the local user is typing in the subscribed
    /// channel — triggers userTyping() on every other subscriber.
    void sendTyping();

    /// Requests editing message @p id (must be this user's own) to
    /// @p newBody.
    void sendEditMessage(qint64 id, const QString& newBody);

    /// Requests deleting message @p id (must be this user's own).
    void sendDeleteMessage(qint64 id);

signals:
    /// Emitted once chat-service confirms the subscription.
    void subscribed(qint64 channelId);

    /// Emitted for every message broadcast on the subscribed channel.
    /// @p attachmentId is -1 and @p attachmentFilename empty when the
    /// message has no attachment (issue #116).
    void messageReceived(qint64 id, const QString& author, const QString& body, const QString& sentAt,
                          qint64 attachmentId, const QString& attachmentFilename);

    /// A message was edited — @p editedAt is the new edit timestamp
    /// (Postgres-serialized, same shape as sentAt).
    void messageEdited(qint64 id, const QString& newBody, const QString& editedAt);

    /// A message was deleted.
    void messageDeleted(qint64 id);

    void errorOccurred(const QString& message);

    /// Reply to joinCall(): existing call participants, not including self.
    void callRosterReceived(const QStringList& participants);

    /// Another participant joined the call.
    void callPeerJoined(const QString& login);

    /// A participant left the call (explicit leaveCall() or disconnect).
    void callPeerLeft(const QString& login);

    /// Signaling payload relayed from another call participant.
    void callSignalReceived(const QString& from, const QJsonObject& payload);

    /// Another subscriber is typing in the subscribed channel.
    void userTyping(const QString& login);

private:
    void onConnected();
    void onTextMessageReceived(const QString& message);

    QUrl webSocketUrl_;
    QWebSocket webSocket_;
    QString pendingToken_;
    qint64 pendingChannelId_ = 0;
};

}  // namespace devicehub
