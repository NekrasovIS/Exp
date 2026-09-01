#pragma once

#include <optional>
#include <string>

namespace user_service {

/// A user's public profile fields (issue #110) — displayName/avatarUrl/
/// publicKey are unset (std::nullopt) rather than empty strings when the
/// user hasn't set them, so callers can tell "never set" apart from
/// "explicitly cleared".
struct Profile {
    std::string login;
    std::optional<std::string> displayName;
    std::optional<std::string> avatarUrl;
    /// Base64-encoded X25519 public key (issue #136) — the private half
    /// never leaves the client; this is only ever a lookup value for
    /// other clients to encrypt to this user, never used server-side.
    std::optional<std::string> publicKey;
};

/// Fields updateProfile() may change — grouped per CLAUDE.md's "prefer
/// fewer function arguments" rule rather than growing updateProfile()'s
/// own parameter list further.
struct ProfileUpdate {
    std::optional<std::string> displayName;
    std::optional<std::string> avatarUrl;
    std::optional<std::string> publicKey;
};

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

    /// @return @p login's profile, or std::nullopt if no such user (issue #110).
    [[nodiscard]] std::optional<Profile> findProfile(const std::string& login);

    /// Overwrites @p login's display_name/avatar_url/public_key. @return
    /// False if no such user exists.
    [[nodiscard]] bool updateProfile(const std::string& login, const ProfileUpdate& update);

private:
    std::string connectionString_;
};

}  // namespace user_service
