#include "chat/ChatClient.h"

#include <QJsonArray>
#include <QJsonDocument>

namespace devicehub {

ChatClient::ChatClient(QUrl webSocketUrl, QObject* parent)
    : QObject(parent), webSocketUrl_(std::move(webSocketUrl)) {
    connect(&webSocket_, &QWebSocket::connected, this, &ChatClient::onConnected);
    connect(&webSocket_, &QWebSocket::textMessageReceived, this, &ChatClient::onTextMessageReceived);
    connect(&webSocket_, &QWebSocket::errorOccurred, this,
            [this](QAbstractSocket::SocketError) { emit errorOccurred(webSocket_.errorString()); });
}

void ChatClient::connectToChannel(const QString& token, qint64 channelId) {
    pendingToken_ = token;
    pendingChannelId_ = channelId;
    webSocket_.open(webSocketUrl_);
}

void ChatClient::onConnected() {
    const QJsonObject hello{{"token", pendingToken_}, {"channel_id", pendingChannelId_}};
    webSocket_.sendTextMessage(QString::fromUtf8(QJsonDocument(hello).toJson(QJsonDocument::Compact)));
}

void ChatClient::onTextMessageReceived(const QString& message) {
    const QJsonDocument document = QJsonDocument::fromJson(message.toUtf8());
    if (!document.isObject()) {
        emit errorOccurred(QStringLiteral("Malformed message from chat-service"));
        return;
    }

    const QJsonObject object = document.object();
    if (object.contains("error")) {
        emit errorOccurred(object.value("error").toString());
    } else if (object.contains("subscribed")) {
        emit subscribed(object.value("channel_id").toVariant().toLongLong());
    } else if (object.contains("call_roster")) {
        QStringList participants;
        const QJsonArray roster = object.value("call_roster").toArray();
        participants.reserve(roster.size());
        for (const QJsonValue& login : roster) {
            participants.append(login.toString());
        }
        emit callRosterReceived(participants);
    } else if (object.contains("call_peer_joined")) {
        emit callPeerJoined(object.value("call_peer_joined").toString());
    } else if (object.contains("call_peer_left")) {
        emit callPeerLeft(object.value("call_peer_left").toString());
    } else if (object.contains("call_signal")) {
        const QJsonObject signal = object.value("call_signal").toObject();
        emit callSignalReceived(signal.value("from").toString(), signal.value("payload").toObject());
    } else if (object.contains("user_typing")) {
        emit userTyping(object.value("user_typing").toString());
    } else if (object.contains("message_edited")) {
        const QJsonObject edited = object.value("message_edited").toObject();
        emit messageEdited(edited.value("id").toVariant().toLongLong(), edited.value("body").toString(),
                            edited.value("edited_at").toString());
    } else if (object.contains("message_deleted")) {
        emit messageDeleted(object.value("message_deleted").toObject().value("id").toVariant().toLongLong());
    } else if (object.contains("author") && object.contains("body")) {
        const QJsonValue attachmentIdValue = object.value("attachment_id");
        emit messageReceived(object.value("id").toVariant().toLongLong(), object.value("author").toString(),
                              object.value("body").toString(), object.value("sent_at").toString(),
                              attachmentIdValue.isNull() ? -1 : attachmentIdValue.toVariant().toLongLong(),
                              object.value("attachment_filename").toString());
    }
}

void ChatClient::sendMessage(const QString& body, qint64 attachmentId) {
    QJsonObject message{{"body", body}};
    if (attachmentId >= 0) {
        message.insert("attachment_id", attachmentId);
    }
    webSocket_.sendTextMessage(QString::fromUtf8(QJsonDocument(message).toJson(QJsonDocument::Compact)));
}

void ChatClient::disconnectFromChannel() {
    webSocket_.close();
}

void ChatClient::joinCall() {
    const QJsonObject message{{"call_join", true}};
    webSocket_.sendTextMessage(QString::fromUtf8(QJsonDocument(message).toJson(QJsonDocument::Compact)));
}

void ChatClient::leaveCall() {
    const QJsonObject message{{"call_leave", true}};
    webSocket_.sendTextMessage(QString::fromUtf8(QJsonDocument(message).toJson(QJsonDocument::Compact)));
}

void ChatClient::sendCallSignal(const QString& to, const QJsonObject& payload) {
    const QJsonObject message{{"call_signal", QJsonObject{{"to", to}, {"payload", payload}}}};
    webSocket_.sendTextMessage(QString::fromUtf8(QJsonDocument(message).toJson(QJsonDocument::Compact)));
}

void ChatClient::sendTyping() {
    const QJsonObject message{{"typing", true}};
    webSocket_.sendTextMessage(QString::fromUtf8(QJsonDocument(message).toJson(QJsonDocument::Compact)));
}

void ChatClient::sendEditMessage(qint64 id, const QString& newBody) {
    const QJsonObject message{{"edit_message", QJsonObject{{"id", id}, {"body", newBody}}}};
    webSocket_.sendTextMessage(QString::fromUtf8(QJsonDocument(message).toJson(QJsonDocument::Compact)));
}

void ChatClient::sendDeleteMessage(qint64 id) {
    const QJsonObject message{{"delete_message", QJsonObject{{"id", id}}}};
    webSocket_.sendTextMessage(QString::fromUtf8(QJsonDocument(message).toJson(QJsonDocument::Compact)));
}

}  // namespace devicehub
