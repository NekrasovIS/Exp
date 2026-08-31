#include "chat/ChatRestClient.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrlQuery>

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
        items.push_back(ChatItem{.id = object.value("id").toVariant().toLongLong(),
                                  .name = object.value("name").toString(),
                                  .ownerLogin = object.value("owner").toString()});
    }
    return items;
}

// chat-service reports the actual failure reason (e.g. "no such community,
// or channel name already taken") in the response body — falling back to
// reply->errorString() alone only ever shows a generic "server replied:
// Not Found", which hides why the request actually failed.
QString extractErrorMessage(QNetworkReply* reply) {
    const QJsonDocument errorBody = QJsonDocument::fromJson(reply->readAll());
    const QString detail = errorBody.isObject() ? errorBody.object().value("error").toString() : QString();
    return detail.isEmpty() ? reply->errorString() : detail;
}

}  // namespace

ChatRestClient::ChatRestClient(QUrl baseUrl, QObject* parent) : QObject(parent), baseUrl_(std::move(baseUrl)) {}

void ChatRestClient::createCommunity(const QString& token, const QString& name) {
    QNetworkReply* reply = networkManager_.post(buildRequest(baseUrl_.resolved(QUrl(QStringLiteral("/communities"))), token),
                                                 QJsonDocument(QJsonObject{{"name", name}}).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit errorOccurred(extractErrorMessage(reply));
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
            emit errorOccurred(extractErrorMessage(reply));
            return;
        }
        emit communitiesListed(parseItemList(reply->readAll()));
    });
}

void ChatRestClient::renameCommunity(const QString& token, qint64 communityId, const QString& newName) {
    const QUrl url = baseUrl_.resolved(QUrl(QStringLiteral("/communities/%1").arg(communityId)));
    QNetworkReply* reply = networkManager_.sendCustomRequest(
        buildRequest(url, token), "PATCH", QJsonDocument(QJsonObject{{"name", newName}}).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply, communityId, newName]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit errorOccurred(extractErrorMessage(reply));
            return;
        }
        emit communityRenamed(communityId, newName);
    });
}

void ChatRestClient::deleteCommunity(const QString& token, qint64 communityId) {
    const QUrl url = baseUrl_.resolved(QUrl(QStringLiteral("/communities/%1").arg(communityId)));
    QNetworkReply* reply = networkManager_.deleteResource(buildRequest(url, token));
    connect(reply, &QNetworkReply::finished, this, [this, reply, communityId]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit errorOccurred(extractErrorMessage(reply));
            return;
        }
        emit communityDeleted(communityId);
    });
}

void ChatRestClient::joinCommunity(const QString& token, qint64 communityId) {
    const QUrl url = baseUrl_.resolved(QUrl(QStringLiteral("/communities/%1/join").arg(communityId)));
    QNetworkReply* reply = networkManager_.post(buildRequest(url, token), QByteArray());
    connect(reply, &QNetworkReply::finished, this, [this, reply, communityId]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit errorOccurred(extractErrorMessage(reply));
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
            emit errorOccurred(extractErrorMessage(reply));
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
            emit errorOccurred(extractErrorMessage(reply));
            return;
        }
        emit channelsListed(parseItemList(reply->readAll()));
    });
}

void ChatRestClient::renameChannel(const QString& token, qint64 channelId, const QString& newName) {
    const QUrl url = baseUrl_.resolved(QUrl(QStringLiteral("/channels/%1").arg(channelId)));
    QNetworkReply* reply = networkManager_.sendCustomRequest(
        buildRequest(url, token), "PATCH", QJsonDocument(QJsonObject{{"name", newName}}).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply, channelId, newName]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit errorOccurred(extractErrorMessage(reply));
            return;
        }
        emit channelRenamed(channelId, newName);
    });
}

void ChatRestClient::listMessages(const QString& token, qint64 channelId, int limit, qint64 beforeId) {
    QUrl url = baseUrl_.resolved(QUrl(QStringLiteral("/channels/%1/messages").arg(channelId)));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("limit"), QString::number(limit));
    if (beforeId >= 0) {
        query.addQueryItem(QStringLiteral("before_id"), QString::number(beforeId));
    }
    url.setQuery(query);

    QNetworkReply* reply = networkManager_.get(buildRequest(url, token));
    connect(reply, &QNetworkReply::finished, this, [this, reply, channelId]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit errorOccurred(extractErrorMessage(reply));
            return;
        }
        QList<ChatMessageInfo> messages;
        const QJsonDocument document = QJsonDocument::fromJson(reply->readAll());
        if (document.isArray()) {
            for (const QJsonValue& value : document.array()) {
                const QJsonObject object = value.toObject();
                messages.push_back(ChatMessageInfo{.id = object.value("id").toVariant().toLongLong(),
                                                     .author = object.value("author").toString(),
                                                     .body = object.value("body").toString(),
                                                     .sentAt = object.value("sent_at").toString()});
            }
        }
        emit messagesListed(channelId, messages);
    });
}

void ChatRestClient::deleteChannel(const QString& token, qint64 channelId) {
    const QUrl url = baseUrl_.resolved(QUrl(QStringLiteral("/channels/%1").arg(channelId)));
    QNetworkReply* reply = networkManager_.deleteResource(buildRequest(url, token));
    connect(reply, &QNetworkReply::finished, this, [this, reply, channelId]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit errorOccurred(extractErrorMessage(reply));
            return;
        }
        emit channelDeleted(channelId);
    });
}

}  // namespace devicehub
