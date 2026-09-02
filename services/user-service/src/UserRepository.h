#pragma once

#include <optional>
#include <string>
#include <utility>

namespace user_service {

/// Поля публичного профиля пользователя (issue #110) — displayName/avatarUrl/
/// publicKey не заданы (std::nullopt), а не являются пустыми строками, когда
/// пользователь их не устанавливал, поэтому вызывающий код может отличить
/// "никогда не задавалось" от "явно очищено".
struct Profile {
    std::string login;
    std::optional<std::string> displayName;
    std::optional<std::string> avatarUrl;
    /// Публичный ключ X25519 в base64 (issue #136) — приватная половина
    /// никогда не покидает клиент; это значение используется только для
    /// поиска другими клиентами, чтобы шифровать для этого пользователя,
    /// и никогда не используется на стороне сервера.
    std::optional<std::string> publicKey;
    /// Задан, если пользователь подключил вход по одноразовому коду
    /// (issue #156); не задан — значит, вход по OTP пока недоступен,
    /// только по паролю.
    std::optional<std::string> email;
};

/// Поля, которые может изменить updateProfile() — сгруппированы согласно
/// правилу CLAUDE.md "предпочитать меньшее количество аргументов функций",
/// вместо того чтобы дальше расширять список параметров самого updateProfile().
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
 * @brief Хранилище учётных записей пользователей на базе Postgres (libpqxx).
 *
 * Открывает новое соединение на каждый вызов, а не использует пул —
 * pqxx::connection не потокобезопасен, а HttpServer может диспетчеризовать
 * обработчики из нескольких потоков; пул соединений — разумное развитие
 * этого решения, когда сервису потребуется выдерживать реальную нагрузку
 * (см. CLAUDE.md, раздел про тестирование производительности).
 */
class UserRepository {
public:
    explicit UserRepository(std::string connectionString);

    /// @return True, если пользователь был создан; false, если @p login уже занят.
    [[nodiscard]] bool createUser(const std::string& login, const std::string& passwordHash);

    /// @return Сохранённый хеш пароля для @p login, или std::nullopt, если такого пользователя нет.
    [[nodiscard]] std::optional<std::string> findPasswordHash(const std::string& login);

    /// @return Профиль @p login, или std::nullopt, если такого пользователя нет (issue #110).
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
