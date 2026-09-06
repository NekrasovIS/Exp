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
 *        POST /users/register и POST /users/resolve-otp-identifier
 *        (issue #156).
 *
 * Fail closed: любая сетевая/протокольная ошибка трактуется как "не
 * подтверждено"/"не зарегистрировано"/"не найдено", а не пробрасывает
 * исключение в обработчик запроса.
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

private:
    std::string host_;
    int port_;
};

}  // namespace auth_service
