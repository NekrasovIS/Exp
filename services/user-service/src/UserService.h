#pragma once

#include <optional>
#include <string>

#include "UserRepository.h"

namespace user_service {

/// Ответ setupTotp() (issue #388) — то, что клиенту нужно, чтобы
/// показать QR-код/принять секрет вручную.
struct TotpSetupInfo {
    /// base32 (не base64 — см. base32.h) — формат, который реально
    /// принимают приложения-аутентификаторы для ручного ввода.
    std::string secretBase32;
    /// `otpauth://totp/DeviceHub:<login>?secret=...&issuer=DeviceHub...`
    /// — клиент кодирует это в QR-код сам, сервер только строит строку.
    std::string otpauthUrl;
};

/// @see UserService::confirmTotp().
enum class ConfirmTotpResult {
    kConfirmed,
    kInvalidCode,
    /// Не было предшествующего вызова setupTotp() (или он истёк/был
    /// перезаписан новым setupTotp() до того, как этот код был введён).
    kNoPendingSetup,
};

/// Полный результат confirmTotp() — backupCodes заполнен только при
/// kConfirmed, и ровно один раз: после этого хранятся только их хеши.
struct ConfirmTotpOutcome {
    ConfirmTotpResult result;
    std::vector<std::string> backupCodes;
};

/// @see UserService::disableTotp().
enum class DisableTotpResult {
    kDisabled,
    kInvalidCode,
    kNotEnabled,
};

/**
 * @brief Бизнес-логика регистрации и входа: хеширует/проверяет пароли
 *        (password_hash) и делегирует хранение UserRepository.
 */
class UserService {
public:
    explicit UserService(UserRepository& repository);

    /// @return True, если @p login был свободен и учётная запись создана.
    [[nodiscard]] bool registerUser(const std::string& login, const std::string& password);

    /// @return True, если @p login существует и @p password соответствует его хешу.
    [[nodiscard]] bool verifyCredentials(const std::string& login, const std::string& password);

    /// @return Профиль @p login, или std::nullopt, если такого пользователя нет (issue #110).
    [[nodiscard]] std::optional<Profile> getProfile(const std::string& login);

    /// Перезаписывает display_name/avatar_url/public_key/email для @p login.
    [[nodiscard]] UpdateProfileResult updateProfile(const std::string& login, const ProfileUpdate& update);

    /// См. UserRepository::resolveOtpIdentifier() (issue #156/#174).
    [[nodiscard]] std::optional<OtpIdentity> resolveOtpIdentifier(const std::string& identifier);

    /// См. UserRepository::sendFriendRequest() (issue #187).
    [[nodiscard]] SendFriendRequestResult sendFriendRequest(const std::string& requesterLogin,
                                                             const std::string& recipientLogin);
    /// См. UserRepository::respondToFriendRequest().
    [[nodiscard]] RespondToFriendRequestResult respondToFriendRequest(std::int64_t requestId,
                                                                       const std::string& recipientLogin,
                                                                       bool accept);
    /// См. UserRepository::listIncomingFriendRequests().
    [[nodiscard]] std::vector<FriendRequestInfo> listIncomingFriendRequests(const std::string& login);
    /// См. UserRepository::listFriends().
    [[nodiscard]] std::vector<std::string> listFriends(const std::string& login);
    /// См. UserRepository::removeFriend().
    [[nodiscard]] bool removeFriend(const std::string& loginA, const std::string& loginB);
    /// См. UserRepository::areFriends() (issue #187, Фаза 2).
    [[nodiscard]] bool areFriends(const std::string& loginA, const std::string& loginB);

    /// Начинает настройку TOTP (issue #388) — генерирует новый секрет,
    /// сохраняет его ещё не включённым. @return std::nullopt, если TOTP
    /// уже включён (сначала нужно отключить через disableTotp()).
    [[nodiscard]] std::optional<TotpSetupInfo> setupTotp(const std::string& login);

    /// Проверяет @p code против секрета, ожидающего подтверждения (см.
    /// setupTotp()) — при совпадении включает TOTP и генерирует 10
    /// одноразовых backup-кодов, отдавая их открытым текстом ровно один
    /// раз (после confirmTotp() хранятся только их хеши).
    [[nodiscard]] ConfirmTotpOutcome confirmTotp(const std::string& login, const std::string& code);

    /// Проверяет @p code против уже включённого секрета — при
    /// совпадении отключает TOTP (удаляет секрет и все backup-коды).
    [[nodiscard]] DisableTotpResult disableTotp(const std::string& login, const std::string& code);

    /// @return True, если TOTP включён (подтверждён) для @p login.
    [[nodiscard]] bool isTotpEnabled(const std::string& login);

    /// Проверяет @p code как текущий TOTP-код ЛИБО как ещё не
    /// использованный backup-код (в этом порядке) — вызывается
    /// auth-service через POST /users/verify-totp при входе, никогда
    /// напрямую клиентами. @return false также если TOTP не включён.
    [[nodiscard]] bool verifyTotpOrBackupCode(const std::string& login, const std::string& code);

private:
    UserRepository& repository_;
};

}  // namespace user_service
