#pragma once

#include <optional>
#include <string>

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
};

/// Поля, которые может изменить updateProfile() — сгруппированы согласно
/// правилу CLAUDE.md "предпочитать меньшее количество аргументов функций",
/// вместо того чтобы дальше расширять список параметров самого updateProfile().
struct ProfileUpdate {
    std::optional<std::string> displayName;
    std::optional<std::string> avatarUrl;
    std::optional<std::string> publicKey;
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

    /// Перезаписывает display_name/avatar_url/public_key для @p login. @return
    /// False, если такого пользователя не существует.
    [[nodiscard]] bool updateProfile(const std::string& login, const ProfileUpdate& update);

private:
    std::string connectionString_;
};

}  // namespace user_service
