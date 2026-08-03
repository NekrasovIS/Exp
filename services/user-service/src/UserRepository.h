#pragma once

#include <optional>
#include <string>

namespace user_service {

/**
 * @brief Postgres-backed storage for user accounts (libpqxx).
 *
 * Opens a fresh connection per call rather than pooling — pqxx::connection
 * isn't thread-safe and HttpServer may dispatch handlers from multiple
 * threads; a connection pool is a reasonable follow-up once this service
 * needs to handle real load (see CLAUDE.md, testing-for-performance).
 */
class UserRepository {
public:
    explicit UserRepository(std::string connectionString);

    /// @return True if the user was created; false if @p login is already taken.
    [[nodiscard]] bool createUser(const std::string& login, const std::string& passwordHash);

    /// @return The stored password hash for @p login, or std::nullopt if no such user.
    [[nodiscard]] std::optional<std::string> findPasswordHash(const std::string& login);

private:
    std::string connectionString_;
};

}  // namespace user_service
