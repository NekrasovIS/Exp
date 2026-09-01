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
    /// Base64-encoded X25519 public key (issue #136), empty if the user
    /// hasn't published one yet.
    QString publicKey;
    /// Issue #156: empty until the user sets one — required before
    /// one-time-code login can be used via email, otherwise unused
    /// client-side.
    QString email;
    /// Issue #174: empty until the user links their Telegram — an
    /// alternative one-time-code delivery channel, preferred over email
    /// server-side when both are set.
    QString telegramChatId;
};

/// Fields updateOwnProfile() may change — grouped per CLAUDE.md's
/// "prefer fewer function arguments" rule (displayName/avatarUrl/email/
/// telegramChatId are four same-typed QStrings, easy to swap by
/// position at the call site) rather than growing updateOwnProfile()'s
/// own parameter list.
struct ProfileEdits {
    QString displayName;
    QString avatarUrl;
    QString email;
    QString telegramChatId;
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
    void updateOwnProfile(const QString& token, const ProfileEdits& edits);

    /// Publishes the caller's X25519 public key (issue #136) — a separate
    /// method from updateOwnProfile() rather than a third parameter on it:
    /// this is driven by IdentityKeyStore at login time, a different
    /// caller/trigger than the user-initiated profile-edit flow.
    void publishPublicKey(const QString& token, const QString& publicKey);

signals:
    void profileReceived(const UserProfile& profile);
    void profileUpdated(const UserProfile& profile);
    void errorOccurred(const QString& message);

private:
    QUrl baseUrl_;
    QNetworkAccessManager networkManager_;
};

}  // namespace devicehub
