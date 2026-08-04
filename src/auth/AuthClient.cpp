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

void AuthClient::registerUser(const QString& login, const QString& password) {
    QNetworkRequest request(baseUrl_.resolved(QUrl(QStringLiteral("/auth/register"))));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));

    const QJsonObject body{{"login", login}, {"password", password}};
    QNetworkReply* reply = networkManager_.post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();

        // Both success (201) and "login taken" (409) come back as a JSON
        // body with a "registered" field — check that before falling back
        // to reply->error(), which QNetworkReply also sets for the 409
        // case even though it's a normal, expected outcome here.
        const QJsonDocument response = QJsonDocument::fromJson(reply->readAll());
        if (response.isObject() && response.object().contains("registered")) {
            const QJsonObject object = response.object();
            const bool registered = object.value("registered").toBool();
            emit registrationCompleted(registered);
            if (registered && object.contains("token")) {
                emit tokenReceived(object.value("token").toString());
            }
            return;
        }

        emit errorOccurred(reply->error() != QNetworkReply::NoError ? reply->errorString()
                                                                      : tr("Malformed response from auth-service"));
    });
}

}  // namespace devicehub
