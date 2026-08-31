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

bool UserService::updateProfile(const std::string& login, const std::optional<std::string>& displayName,
                                 const std::optional<std::string>& avatarUrl) {
    return repository_.updateProfile(login, displayName, avatarUrl);
}

}  // namespace user_service
