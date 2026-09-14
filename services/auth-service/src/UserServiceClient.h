#pragma once

#include <optional>
#include <string>

namespace auth_service {

/// Профиль, к которому можно доставить OTP-код (issue #156/#174) —
/// зеркало user_service::OtpIdentity на стороне auth-service (сюда
/// приходит уже как JSON-ответ POST /users/resolve-otp-identifier, а не
/// напрямую из БД). Сгруппированы в структуру, а не std::pair из двух
/// std::string подряд, которые легко перепутать местами.
struct OtpIdentity {
    std::string login;
    std::optional<std::string> email;
    std::optional<std::string> telegramChatId;
};

/**
 * @brief Вызывает user-service: POST /users/verify-credentials,
 *        POST /users/register, POST /users/resolve-otp-identifier
 *        (issue #156), GET /internal/totp-status и
 *        POST /users/verify-totp (issue #388/#389).
 *
 * Fail closed: любая сетевая/протокольная ошибка трактуется как "не
 * подтверждено"/"не зарегистрировано"/"не найдено"/"выключено"/"неверный
 * код", а не пробрасывает исключение в обработчик запроса.
 */
class UserServiceClient {
public:
    UserServiceClient(std::string host, int port);

    [[nodiscard]] bool verifyCredentials(const std::string& login, const std::string& password) const;

    /// Вызывает POST /users/register.
    /// @return true, если login был свободен и аккаунт создан.
    [[nodiscard]] bool registerUser(const std::string& login, const std::string& password) const;

    /// Вызывает POST /users/resolve-otp-identifier — приводит
    /// @p identifier (login, email или Telegram chat_id) к OtpIdentity
    /// для входа по одноразовому коду (issue #156/#174). @return
    /// std::nullopt, если такого пользователя нет или у него не задано
    /// ни одного канала доставки.
    [[nodiscard]] std::optional<OtpIdentity> resolveOtpIdentifier(const std::string& identifier) const;

    /// Вызывает GET /internal/totp-status?login= (issue #388/#389) —
    /// без аутентификации, как и /internal/friendship: спрашивает от
    /// имени ещё не аутентифицированного пользователя, которому
    /// нечем себя авторизовать. Fail closed в сторону "выключено": сетевая
    /// ошибка не должна заблокировать вход пользователям без 2FA.
    [[nodiscard]] bool isTotpEnabled(const std::string& login) const;

    /// Вызывает POST /users/verify-totp — @p code принимается и как
    /// TOTP-код, и как backup-код (issue #388). Fail closed: сетевая
    /// ошибка трактуется как неверный код, а не как "пропустить проверку".
    [[nodiscard]] bool verifyTotp(const std::string& login, const std::string& code) const;

private:
    std::string host_;
    int port_;
};

}  // namespace auth_service
