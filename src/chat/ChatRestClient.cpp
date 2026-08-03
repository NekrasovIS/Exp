#include "chat/ChatRestClient.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>

namespace devicehub {

namespace {

QNetworkRequest buildRequest(const QUrl& url, const QString& token) {
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("Authorization", "Bearer " + token.toUtf8());
    return request;
}

QList<ChatItem> parseItemList(const QByteArray& jsonBytes) {
    QList<ChatItem> items;
    const QJsonDocument document = QJsonDocument::fromJson(jsonBytes);
    if (!document.isArray()) {
        return items;
    }
    for (const QJsonValue& value : document.array()) {
        const QJsonObject object = value.toObject();
        items.push_back(ChatItem{.id = object.value("id").toVariant().toLongLong(), .name = object.value("name").toString()});
    }
    return items;
}

}  // namespace

ChatRestClient::ChatRestClient(QUrl baseUrl, QObject* parent) : QObject(parent), baseUrl_(std::move(baseUrl)) {}

void ChatRestClient::createCommunity(const QString& token, const QString& name) {
    QNetworkReply* reply = networkManager_.post(buildRequest(baseUrl_.resolved(QUrl(QStringLiteral("/communities"))), token),
                                                 QJsonDocument(QJsonObject{{"name", name}}).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit errorOccurred(reply->errorString());
            return;
        }
        const QJsonObject object = QJsonDocument::fromJson(reply->readAll()).object();
        emit communityCreated(object.value("id").toVariant().toLongLong(), object.value("name").toString());
    });
}

void ChatRestClient::listCommunities(const QString& token) {
    QNetworkReply* reply = networkManager_.get(buildRequest(baseUrl_.resolved(QUrl(QStringLiteral("/communities"))), token));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit errorOccurred(reply->errorString());
            return;
        }
        emit communitiesListed(parseItemList(reply->readAll()));
    });
}

void ChatRestClient::joinCommunity(const QString& token, qint64 communityId) {
    const QUrl url = baseUrl_.resolved(QUrl(QStringLiteral("/communities/%1/join").arg(communityId)));
    QNetworkReply* reply = networkManager_.post(buildRequest(url, token), QByteArray());
    connect(reply, &QNetworkReply::finished, this, [this, reply, communityId]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit errorOccurred(reply->errorString());
            return;
        }
        emit communityJoined(communityId);
    });
}

void ChatRestClient::createChannel(const QString& token, qint64 communityId, const QString& name) {
    const QUrl url = baseUrl_.resolved(QUrl(QStringLiteral("/communities/%1/channels").arg(communityId)));
    QNetworkReply* reply =
        networkManager_.post(buildRequest(url, token), QJsonDocument(QJsonObject{{"name", name}}).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply, name]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit errorOccurred(reply->errorString());
            return;
        }
        const QJsonObject object = QJsonDocument::fromJson(reply->readAll()).object();
        emit channelCreated(object.value("id").toVariant().toLongLong(), name);
    });
}

void ChatRestClient::listChannels(const QString& token, qint64 communityId) {
    const QUrl url = baseUrl_.resolved(QUrl(QStringLiteral("/communities/%1/channels").arg(communityId)));
    QNetworkReply* reply = networkManager_.get(buildRequest(url, token));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit errorOccurred(reply->errorString());
            return;
        }
        emit channelsListed(parseItemList(reply->readAll()));
    });
}

}  // namespace devicehub
