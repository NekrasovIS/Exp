#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

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
    /// через email (issue #156); не задан — значит, этот канал OTP
    /// пока недоступен для аккаунта.
    std::optional<std::string> email;
    /// chat_id чата пользователя с Telegram-ботом DeviceHub (issue
    /// #174) — альтернативный/предпочтительный канал доставки OTP-кода,
    /// когда задан оба сразу с email.
    std::optional<std::string> telegramChatId;
};

/// Поля, которые может изменить updateProfile() — сгруппированы согласно
/// правилу CLAUDE.md "предпочитать меньшее количество аргументов функций",
/// вместо того чтобы дальше расширять список параметров самого updateProfile().
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

/// Заявка в друзья, как её возвращает listIncomingFriendRequests()
/// (issue #187, Фаза 1).
struct FriendRequestInfo {
    std::int64_t id = 0;
    std::string requesterLogin;
    std::string createdAt;
};

/// @see UserRepository::sendFriendRequest().
enum class SendFriendRequestResult {
    kSent,
    /// У получателя уже была pending-заявка отправителю — вместо
    /// второй записи она сразу принимается, и стороны становятся
    /// друзьями за один вызов.
    kAutoAccepted,
    kAlreadyFriends,
    kAlreadyRequested,
    kNoSuchRecipient,
    kCannotFriendSelf,
};

/// @see UserRepository::respondToFriendRequest().
enum class RespondToFriendRequestResult {
    kAccepted,
    kDeclined,
    kNoSuchRequest,
    /// Заявка существует, но @p recipientLogin — не её адресат.
    kNotYourRequest,
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

    /// Приводит @p identifier — принимается как login, email (issue
    /// #156) или Telegram chat_id (issue #174) — к OtpIdentity, которая
    /// нужна auth-service, чтобы отправить код и затем выдать токен.
    /// @return std::nullopt, если по @p identifier никто не найден ни
    /// по одному из полей, либо найден, но ни email, ни telegram_chat_id
    /// не заданы (отправлять код некуда).
    [[nodiscard]] std::optional<OtpIdentity> resolveOtpIdentifier(const std::string& identifier);

    /// Отправляет заявку в друзья от @p requesterLogin к @p recipientLogin
    /// (issue #187). Взаимные заявки авто-принимаются — см.
    /// SendFriendRequestResult::kAutoAccepted.
    [[nodiscard]] SendFriendRequestResult sendFriendRequest(const std::string& requesterLogin,
                                                             const std::string& recipientLogin);

    /// Принимает (@p accept == true) или отклоняет заявку @p requestId —
    /// только если её адресат — @p recipientLogin.
    [[nodiscard]] RespondToFriendRequestResult respondToFriendRequest(std::int64_t requestId,
                                                                       const std::string& recipientLogin,
                                                                       bool accept);

    /// Входящие pending-заявки для @p login, самые новые первыми.
    [[nodiscard]] std::vector<FriendRequestInfo> listIncomingFriendRequests(const std::string& login);

    /// Логины всех друзей @p login (порядок не гарантирован).
    [[nodiscard]] std::vector<std::string> listFriends(const std::string& login);

    /// @return True, если пара состояла в дружбе и была удалена; false,
    /// если они не были друзьями.
    [[nodiscard]] bool removeFriend(const std::string& loginA, const std::string& loginB);

    /// @return True, если @p loginA и @p loginB — друзья (issue #187,
    /// Фаза 2) — используется через внутренний эндпоинт
    /// GET /internal/friendship, чтобы chat-service мог разрешить
    /// открытие нового диалога личных сообщений только между друзьями.
    [[nodiscard]] bool areFriends(const std::string& loginA, const std::string& loginB);

private:
    std::string connectionString_;
};

}  // namespace user_service
