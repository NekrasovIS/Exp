#include "auth/AuthClient.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>

namespace devicehub {

AuthClient::AuthClient(QUrl baseUrl, QObject* parent) : QObject(parent), baseUrl_(std::move(baseUrl)) {}

void AuthClient::requestToken(const QString& login, const QString& password) {
    QNetworkRequest request(baseUrl_.resolved(QUrl(QStringLiteral("/auth/token"))));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));

    const QJsonObject body{{"login", login}, {"password", password}};
    QNetworkReply* reply = networkManager_.post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            const QJsonDocument errorBody = QJsonDocument::fromJson(reply->readAll());
            const QString detail = errorBody.isObject() ? errorBody.object().value("error").toString() : QString();
            emit errorOccurred(detail.isEmpty() ? reply->errorString() : detail);
            return;
        }

        const QJsonDocument response = QJsonDocument::fromJson(reply->readAll());
        if (!response.isObject() || !response.object().contains("token")) {
            emit errorOccurred(QStringLiteral("Malformed response from auth-service"));
            return;
        }

        emit tokenReceived(response.object().value("token").toString());
    });
}

void AuthClient::verifyToken(const QString& token) {
    QNetworkRequest request(baseUrl_.resolved(QUrl(QStringLiteral("/auth/verify"))));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));

    const QJsonObject body{{"token", token}};
    QNetworkReply* reply = networkManager_.post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit errorOccurred(reply->errorString());
            return;
        }

        const QJsonDocument response = QJsonDocument::fromJson(reply->readAll());
        if (!response.isObject() || !response.object().contains("valid")) {
            emit errorOccurred(QStringLiteral("Malformed response from auth-service"));
            return;
        }

        const QJsonObject object = response.object();
        emit tokenVerified(object.value("valid").toBool(), object.value("subject").toString());
    });
}

}  // namespace devicehub
