#include "user/UserProfileClient.h"

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

UserProfile parseProfile(const QByteArray& jsonBytes) {
    const QJsonObject object = QJsonDocument::fromJson(jsonBytes).object();
    return UserProfile{.login = object.value("login").toString(),
                        .displayName = object.value("display_name").toString(),
                        .avatarUrl = object.value("avatar_url").toString(),
                        .publicKey = object.value("public_key").toString(),
                        .email = object.value("email").toString(),
                        .telegramChatId = object.value("telegram_chat_id").toString()};
}

// user-service сообщает настоящую причину сбоя (например, «нет такого
// пользователя») в теле ответа — откат к одному лишь
// reply->errorString() всегда показывает лишь общее «server replied:
// Not Found».
QString extractErrorMessage(QNetworkReply* reply) {
    const QJsonDocument errorBody = QJsonDocument::fromJson(reply->readAll());
    const QString detail = errorBody.isObject() ? errorBody.object().value("error").toString() : QString();
    return detail.isEmpty() ? reply->errorString() : detail;
}

}  // namespace

UserProfileClient::UserProfileClient(QUrl baseUrl, QObject* parent) : QObject(parent), baseUrl_(std::move(baseUrl)) {}

void UserProfileClient::fetchProfile(const QString& token, const QString& login) {
    const QUrl url = baseUrl_.resolved(QUrl(QStringLiteral("/users/%1/profile").arg(login)));
    QNetworkReply* reply = networkManager_.get(buildRequest(url, token));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit errorOccurred(extractErrorMessage(reply));
            return;
        }
        emit profileReceived(parseProfile(reply->readAll()));
    });
}

void UserProfileClient::updateOwnProfile(const QString& token, const ProfileEdits& edits) {
    const QUrl url = baseUrl_.resolved(QUrl(QStringLiteral("/users/me")));
    const QJsonObject body{{"display_name", edits.displayName},
                            {"avatar_url", edits.avatarUrl},
                            {"email", edits.email},
                            {"telegram_chat_id", edits.telegramChatId}};
    QNetworkReply* reply =
        networkManager_.sendCustomRequest(buildRequest(url, token), "PATCH", QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit errorOccurred(extractErrorMessage(reply));
            return;
        }
        emit profileUpdated(parseProfile(reply->readAll()));
    });
}

void UserProfileClient::publishPublicKey(const QString& token, const QString& publicKey) {
    const QUrl url = baseUrl_.resolved(QUrl(QStringLiteral("/users/me")));
    const QJsonObject body{{"public_key", publicKey}};
    QNetworkReply* reply =
        networkManager_.sendCustomRequest(buildRequest(url, token), "PATCH", QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit errorOccurred(extractErrorMessage(reply));
            return;
        }
        emit profileUpdated(parseProfile(reply->readAll()));
    });
}

}  // namespace devicehub
