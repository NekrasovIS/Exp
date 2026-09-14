#include "user/AvatarCache.h"

#include "user/UserProfileClient.h"

namespace devicehub {

AvatarCache::AvatarCache(UserProfileClient& client, QObject* parent) : QObject(parent), client_(client) {
    connect(&client_, &UserProfileClient::avatarFetched, this,
            [this](const QString& login, const QByteArray& data, const QString& /*contentType*/) {
                pending_.remove(login);
                QImage image;
                if (!image.loadFromData(data)) {
                    failed_.insert(login);
                    return;
                }
                images_.insert(login, image);
                emit avatarReady(login);
            });
    connect(&client_, &UserProfileClient::avatarFetchFailed, this, [this](const QString& login) {
        pending_.remove(login);
        failed_.insert(login);
    });
}

std::optional<QImage> AvatarCache::imageFor(const QString& login) {
    const auto it = images_.find(login);
    if (it != images_.end()) {
        return *it;
    }
    if (!pending_.contains(login) && !failed_.contains(login)) {
        pending_.insert(login);
        client_.fetchAvatar(login);
    }
    return std::nullopt;
}

void AvatarCache::invalidate(const QString& login) {
    images_.remove(login);
    failed_.remove(login);
    pending_.remove(login);
}

}  // namespace devicehub
