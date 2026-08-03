#include "UserRepository.h"

#include <pqxx/pqxx>

namespace user_service {

UserRepository::UserRepository(std::string connectionString) : connectionString_(std::move(connectionString)) {}

bool UserRepository::createUser(const std::string& login, const std::string& passwordHash) {
    pqxx::connection connection(connectionString_);
    pqxx::work transaction(connection);

    try {
        transaction.exec_params("INSERT INTO users (login, password_hash) VALUES ($1, $2)", login, passwordHash);
        transaction.commit();
        return true;
    } catch (const pqxx::unique_violation&) {
        return false;
    }
}

std::optional<std::string> UserRepository::findPasswordHash(const std::string& login) {
    pqxx::connection connection(connectionString_);
    pqxx::work transaction(connection);

    const pqxx::result rows = transaction.exec_params("SELECT password_hash FROM users WHERE login = $1", login);
    if (rows.empty()) {
        return std::nullopt;
    }

    return rows[0][0].as<std::string>();
}

}  // namespace user_service
