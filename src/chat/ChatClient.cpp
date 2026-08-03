#include "chat/ChatClient.h"

#include <QJsonDocument>
#include <QJsonObject>

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
    } else if (object.contains("author") && object.contains("body")) {
        emit messageReceived(object.value("author").toString(), object.value("body").toString(),
                              object.value("sent_at").toString());
    }
}

void ChatClient::sendMessage(const QString& body) {
    const QJsonObject message{{"body", body}};
    webSocket_.sendTextMessage(QString::fromUtf8(QJsonDocument(message).toJson(QJsonDocument::Compact)));
}

void ChatClient::disconnectFromChannel() {
    webSocket_.close();
}

}  // namespace devicehub
