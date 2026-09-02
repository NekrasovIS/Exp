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

    const pqxx::result rows = transaction.exec(
        "SELECT display_name, avatar_url, public_key, email FROM users WHERE login = $1", pqxx::params{login});
    if (rows.empty()) {
        return std::nullopt;
    }

    return Profile{.login = login,
                    .displayName = rows[0][0].is_null() ? std::nullopt : std::make_optional(rows[0][0].as<std::string>()),
                    .avatarUrl = rows[0][1].is_null() ? std::nullopt : std::make_optional(rows[0][1].as<std::string>()),
                    .publicKey = rows[0][2].is_null() ? std::nullopt : std::make_optional(rows[0][2].as<std::string>()),
                    .email = rows[0][3].is_null() ? std::nullopt : std::make_optional(rows[0][3].as<std::string>())};
}

UpdateProfileResult UserRepository::updateProfile(const std::string& login, const ProfileUpdate& update) {
    pqxx::connection connection(connectionString_);
    pqxx::work transaction(connection);

    pqxx::result rows;
    try {
        rows = transaction.exec(
            "UPDATE users SET display_name = $1, avatar_url = $2, public_key = $3, email = $4 WHERE login = $5 "
            "RETURNING id",
            pqxx::params{update.displayName, update.avatarUrl, update.publicKey, update.email, login});
    } catch (const pqxx::unique_violation&) {
        return UpdateProfileResult::kEmailTaken;
    }
    transaction.commit();
    return rows.empty() ? UpdateProfileResult::kNoSuchUser : UpdateProfileResult::kUpdated;
}

std::optional<std::pair<std::string, std::string>> UserRepository::resolveOtpIdentifier(
    const std::string& identifier) {
    pqxx::connection connection(connectionString_);
    pqxx::work transaction(connection);

    const pqxx::result rows = transaction.exec(
        "SELECT login, email FROM users WHERE login = $1 OR email = $1", pqxx::params{identifier});
    if (rows.empty() || rows[0][1].is_null()) {
        return std::nullopt;
    }

    return std::make_pair(rows[0][0].as<std::string>(), rows[0][1].as<std::string>());
}

}  // namespace user_service
