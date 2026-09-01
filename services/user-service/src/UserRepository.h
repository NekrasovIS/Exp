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
    /// Задан, если пользователь подключил вход по одноразовому коду
    /// через email (issue #156); не задан — значит, этот канал OTP
    /// пока недоступен для аккаунта.
    std::optional<std::string> email;
    /// chat_id чата пользователя с Telegram-ботом DeviceHub (issue
    /// #174) — альтернативный/предпочтительный канал доставки OTP-кода,
    /// когда задан оба сразу с email.
    std::optional<std::string> telegramChatId;
};

/// Fields updateProfile() may change — grouped per CLAUDE.md's "prefer
/// fewer function arguments" rule rather than growing updateProfile()'s
/// own parameter list further.
struct ProfileUpdate {
    std::optional<std::string> displayName;
    std::optional<std::string> avatarUrl;
    std::optional<std::string> publicKey;
    std::optional<std::string> email;
    std::optional<std::string> telegramChatId;
};

/// Результат updateProfile() — обычный bool не может различить "нет
/// такого пользователя" и "email/telegram_chat_id уже заняты другим
/// аккаунтом" (ограничения уникальности из issue #156/#174), а
/// вызывающей стороне на них нужно реагировать по-разному (404 vs. 409).
enum class UpdateProfileResult {
    kUpdated,
    kNoSuchUser,
    kEmailTaken,
    kTelegramChatIdTaken,
};

/// Профиль, к которому можно доставить OTP-код (issue #156/#174) —
/// сгруппированы в структуру, а не std::pair/std::tuple из трёх
/// std::string подряд, которые легко перепутать местами (правило
/// проекта про количество/однотипность параметров).
struct OtpIdentity {
    std::string login;
    std::optional<std::string> email;
    std::optional<std::string> telegramChatId;
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

    /// Приводит @p identifier — принимается как login, email (issue
    /// #156) или Telegram chat_id (issue #174) — к OtpIdentity, которая
    /// нужна auth-service, чтобы отправить код и затем выдать токен.
    /// @return std::nullopt, если по @p identifier никто не найден ни
    /// по одному из полей, либо найден, но ни email, ни telegram_chat_id
    /// не заданы (отправлять код некуда).
    [[nodiscard]] std::optional<OtpIdentity> resolveOtpIdentifier(const std::string& identifier);

private:
    std::string connectionString_;
};

}  // namespace user_service
