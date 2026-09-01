#pragma once

#include <optional>
#include <string>
#include <utility>

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
    /// Задан, если пользователь подключил вход по одноразовому коду
    /// (issue #156); не задан — значит, вход по OTP пока недоступен,
    /// только по паролю.
    std::optional<std::string> email;
};

/// Fields updateProfile() may change — grouped per CLAUDE.md's "prefer
/// fewer function arguments" rule rather than growing updateProfile()'s
/// own parameter list further.
struct ProfileUpdate {
    std::optional<std::string> displayName;
    std::optional<std::string> avatarUrl;
    std::optional<std::string> publicKey;
    std::optional<std::string> email;
};

/// Результат updateProfile() — обычный bool не может различить "нет
/// такого пользователя" и "email уже занят другим аккаунтом"
/// (ограничение уникальности email из issue #156), а вызывающей
/// стороне на них нужно реагировать по-разному (404 vs. 409).
enum class UpdateProfileResult {
    kUpdated,
    kNoSuchUser,
    kEmailTaken,
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

    /// Перезаписывает display_name/avatar_url/public_key/email для @p login.
    [[nodiscard]] UpdateProfileResult updateProfile(const std::string& login, const ProfileUpdate& update);

    /// Приводит @p identifier — принимается и как login, и как email
    /// (issue #156, вход по коду через email) — к паре (login, email),
    /// которая нужна auth-service, чтобы отправить код и затем выдать
    /// токен. @return std::nullopt, если по @p identifier никто не
    /// найден ни по одному из полей, либо найден, но email не задан
    /// (отправлять код некуда).
    [[nodiscard]] std::optional<std::pair<std::string, std::string>> resolveOtpIdentifier(
        const std::string& identifier);

private:
    std::string connectionString_;
};

}  // namespace user_service
