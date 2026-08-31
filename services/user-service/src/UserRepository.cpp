#include "UserRepository.h"

#include <pqxx/pqxx>

namespace user_service {

UserRepository::UserRepository(std::string connectionString) : connectionString_(std::move(connectionString)) {}

bool UserRepository::createUser(const std::string& login, const std::string& passwordHash) {
    pqxx::connection connection(connectionString_);
    pqxx::work transaction(connection);

    try {
        transaction.exec("INSERT INTO users (login, password_hash) VALUES ($1, $2)", pqxx::params{login, passwordHash});
        transaction.commit();
        return true;
    } catch (const pqxx::unique_violation&) {
        return false;
    }
}

std::optional<std::string> UserRepository::findPasswordHash(const std::string& login) {
    pqxx::connection connection(connectionString_);
    pqxx::work transaction(connection);

    const pqxx::result rows = transaction.exec("SELECT password_hash FROM users WHERE login = $1", pqxx::params{login});
    if (rows.empty()) {
        return std::nullopt;
    }

    return rows[0][0].as<std::string>();
}

std::optional<Profile> UserRepository::findProfile(const std::string& login) {
    pqxx::connection connection(connectionString_);
    pqxx::work transaction(connection);

    const pqxx::result rows =
        transaction.exec("SELECT display_name, avatar_url FROM users WHERE login = $1", pqxx::params{login});
    if (rows.empty()) {
        return std::nullopt;
    }

    return Profile{.login = login,
                    .displayName = rows[0][0].is_null() ? std::nullopt : std::make_optional(rows[0][0].as<std::string>()),
                    .avatarUrl = rows[0][1].is_null() ? std::nullopt : std::make_optional(rows[0][1].as<std::string>())};
}

bool UserRepository::updateProfile(const std::string& login, const std::optional<std::string>& displayName,
                                    const std::optional<std::string>& avatarUrl) {
    pqxx::connection connection(connectionString_);
    pqxx::work transaction(connection);

    const pqxx::result rows =
        transaction.exec("UPDATE users SET display_name = $1, avatar_url = $2 WHERE login = $3 RETURNING id",
                          pqxx::params{displayName, avatarUrl, login});
    transaction.commit();
    return !rows.empty();
}

}  // namespace user_service
