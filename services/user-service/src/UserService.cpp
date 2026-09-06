#include "UserService.h"

#include "password_hash.h"

namespace user_service {

UserService::UserService(UserRepository& repository) : repository_(repository) {}

bool UserService::registerUser(const std::string& login, const std::string& password) {
    return repository_.createUser(login, password_hash::hash(password));
}

bool UserService::verifyCredentials(const std::string& login, const std::string& password) {
    const std::optional<std::string> storedHash = repository_.findPasswordHash(login);
    if (!storedHash.has_value()) {
        return false;
    }

    return password_hash::verify(*storedHash, password);
}

std::optional<Profile> UserService::getProfile(const std::string& login) {
    return repository_.findProfile(login);
}

UpdateProfileResult UserService::updateProfile(const std::string& login, const ProfileUpdate& update) {
    return repository_.updateProfile(login, update);
}

std::optional<OtpIdentity> UserService::resolveOtpIdentifier(const std::string& identifier) {
    return repository_.resolveOtpIdentifier(identifier);
}

SendFriendRequestResult UserService::sendFriendRequest(const std::string& requesterLogin,
                                                         const std::string& recipientLogin) {
    return repository_.sendFriendRequest(requesterLogin, recipientLogin);
}

RespondToFriendRequestResult UserService::respondToFriendRequest(std::int64_t requestId,
                                                                    const std::string& recipientLogin, bool accept) {
    return repository_.respondToFriendRequest(requestId, recipientLogin, accept);
}

std::vector<FriendRequestInfo> UserService::listIncomingFriendRequests(const std::string& login) {
    return repository_.listIncomingFriendRequests(login);
}

std::vector<std::string> UserService::listFriends(const std::string& login) {
    return repository_.listFriends(login);
}

bool UserService::removeFriend(const std::string& loginA, const std::string& loginB) {
    return repository_.removeFriend(loginA, loginB);
}

}  // namespace user_service
