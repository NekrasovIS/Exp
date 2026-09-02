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

        const QJsonObject object = response.object();
        emit tokenReceived(object.value("token").toString(), object.value("refresh_token").toString(),
                            object.value("expires_at").toVariant().toLongLong());
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

        // И успех (201), и «логин занят» (409) возвращаются как JSON-тело
        // с полем "registered" — проверяем его прежде чем откатываться к
        // reply->error(), которое QNetworkReply также выставляет для
        // случая 409, хотя здесь это нормальный, ожидаемый исход.
        const QJsonDocument response = QJsonDocument::fromJson(reply->readAll());
        if (response.isObject() && response.object().contains("registered")) {
            const QJsonObject object = response.object();
            const bool registered = object.value("registered").toBool();
            emit registrationCompleted(registered);
            if (registered && object.contains("token")) {
                emit tokenReceived(object.value("token").toString(), object.value("refresh_token").toString(),
                                    object.value("expires_at").toVariant().toLongLong());
            }
            return;
        }

        emit errorOccurred(reply->error() != QNetworkReply::NoError ? reply->errorString()
                                                                      : tr("Malformed response from auth-service"));
    });
}

void AuthClient::refreshAccessToken(const QString& refreshToken) {
    QNetworkRequest request(baseUrl_.resolved(QUrl(QStringLiteral("/auth/refresh"))));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));

    const QJsonObject body{{"refresh_token", refreshToken}};
    QNetworkReply* reply = networkManager_.post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));

    connect(reply, &QNetworkReply::finished, this, [this, reply, refreshToken]() {
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

        // /auth/refresh не выпускает новый refresh-токен (они не
        // ротируются — см. doc-комментарий TokenService), поэтому тот, что
        // только что был успешно погашен, остаётся текущим.
        const QJsonObject object = response.object();
        emit tokenReceived(object.value("token").toString(), refreshToken,
                            object.value("expires_at").toVariant().toLongLong());
    });
}

}  // namespace devicehub
