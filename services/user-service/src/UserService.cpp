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

}  // namespace user_service
