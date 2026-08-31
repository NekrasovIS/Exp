#pragma once

#include <QNetworkAccessManager>
#include <QObject>
#include <QUrl>

namespace devicehub {

/// A user's profile as returned by user-service's REST API — displayName/
/// avatarUrl are empty strings when unset (unlike the server-side struct,
/// no null/unset distinction needed on the client: an empty field means
/// "fall back to login" everywhere it's displayed).
struct UserProfile {
    QString login;
    QString displayName;
    QString avatarUrl;
};

/**
 * @brief REST client for user-service's profile endpoints:
 *        GET /users/{login}/profile, PATCH /users/me (issue #110).
 *
 * Deliberately separate from AuthClient (auth-service) and ChatRestClient
 * (chat-service) — user-service owns account/profile data, a different
 * service with its own base URL. Async via signals, like the other REST
 * clients — never blocks the GUI thread on a network round trip.
 */
class UserProfileClient : public QObject {
    Q_OBJECT

public:
    explicit UserProfileClient(QUrl baseUrl, QObject* parent = nullptr);

    /// Fetches @p login's profile.
    void fetchProfile(const QString& token, const QString& login);

    /// Updates the caller's own profile (login comes from @p token
    /// server-side — never spoofable via this call).
    void updateOwnProfile(const QString& token, const QString& displayName, const QString& avatarUrl);

signals:
    void profileReceived(const UserProfile& profile);
    void profileUpdated(const UserProfile& profile);
    void errorOccurred(const QString& message);

private:
    QUrl baseUrl_;
    QNetworkAccessManager networkManager_;
};

}  // namespace devicehub
