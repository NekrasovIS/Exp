#include "UserService.h"

#include "base32.h"
#include "base64.h"
#include "password_hash.h"
#include "totp.h"

#include <sodium.h>

#include <chrono>
#include <cstdint>
#include <string_view>

namespace user_service {

namespace {
constexpr int kBackupCodeCount = 10;
constexpr int kBackupCodeLength = 10;
// Excludes 0/1/O/I/L (issue #388) — a backup code is meant to be
// hand-copied/typed from a screen, and this alphabet has no pair of
// characters a human is likely to misread for each other.
constexpr std::string_view kBackupCodeAlphabet = "23456789ABCDEFGHJKLMNPQRSTUVWXYZ";

int64_t nowUnixSeconds() {
    return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch())
        .count();
}

std::string generateBackupCode() {
    std::string code;
    code.reserve(kBackupCodeLength);
    for (int i = 0; i < kBackupCodeLength; ++i) {
        code.push_back(kBackupCodeAlphabet[randombytes_uniform(static_cast<uint32_t>(kBackupCodeAlphabet.size()))]);
    }
    return code;
}
}  // namespace

UserService::UserService(UserRepository& repository) : repository_(repository) {}

bool UserService::registerUser(const std::string& login, const std::string& password) {
    return repository_.createUser(login, password_hash::hash(password));
}

bool UserService::verifyCredentials(const std::string& login, const std::string& password) {
    const std::optional<std::string> storedHash = repository_.findPasswordHash(login);
    if (!storedHash.has_value()) {
        return false;
    }

    return password_hash::verify(*storedHash, password);
}

std::optional<Profile> UserService::getProfile(const std::string& login) {
    return repository_.findProfile(login);
}

UpdateProfileResult UserService::updateProfile(const std::string& login, const ProfileUpdate& update) {
    return repository_.updateProfile(login, update);
}

std::optional<OtpIdentity> UserService::resolveOtpIdentifier(const std::string& identifier) {
    return repository_.resolveOtpIdentifier(identifier);
}

SendFriendRequestResult UserService::sendFriendRequest(const std::string& requesterLogin,
                                                         const std::string& recipientLogin) {
    return repository_.sendFriendRequest(requesterLogin, recipientLogin);
}

RespondToFriendRequestResult UserService::respondToFriendRequest(std::int64_t requestId,
                                                                    const std::string& recipientLogin, bool accept) {
    return repository_.respondToFriendRequest(requestId, recipientLogin, accept);
}

std::vector<FriendRequestInfo> UserService::listIncomingFriendRequests(const std::string& login) {
    return repository_.listIncomingFriendRequests(login);
}

std::vector<std::string> UserService::listFriends(const std::string& login) {
    return repository_.listFriends(login);
}

bool UserService::removeFriend(const std::string& loginA, const std::string& loginB) {
    return repository_.removeFriend(loginA, loginB);
}

bool UserService::areFriends(const std::string& loginA, const std::string& loginB) {
    return repository_.areFriends(loginA, loginB);
}

std::optional<TotpSetupInfo> UserService::setupTotp(const std::string& login) {
    const std::string secret = totp::generateSecret();
    if (repository_.beginTotpSetup(login, base64::encode(secret)) == BeginTotpSetupResult::kAlreadyEnabled) {
        return std::nullopt;
    }

    const std::string secretBase32 = base32::encode(secret);
    return TotpSetupInfo{.secretBase32 = secretBase32,
                          .otpauthUrl = "otpauth://totp/DeviceHub:" + login + "?secret=" + secretBase32 +
                                        "&issuer=DeviceHub&algorithm=SHA1&digits=6&period=30"};
}

ConfirmTotpOutcome UserService::confirmTotp(const std::string& login, const std::string& code) {
    // A secret exists but is already enabled is not a "pending setup"
    // — confirmTotp() only ever applies to what setupTotp() most
    // recently started, never re-confirms an already-active secret.
    if (repository_.isTotpEnabled(login)) {
        return ConfirmTotpOutcome{.result = ConfirmTotpResult::kNoPendingSetup, .backupCodes = {}};
    }
    const std::optional<std::string> secretBase64 = repository_.findTotpSecret(login);
    const std::optional<std::string> secret =
        secretBase64.has_value() ? base64::decode(*secretBase64) : std::nullopt;
    if (!secret.has_value()) {
        return ConfirmTotpOutcome{.result = ConfirmTotpResult::kNoPendingSetup, .backupCodes = {}};
    }
    if (!totp::verify(*secret, code, nowUnixSeconds())) {
        return ConfirmTotpOutcome{.result = ConfirmTotpResult::kInvalidCode, .backupCodes = {}};
    }

    repository_.confirmTotpSecret(login);

    std::vector<std::string> plainCodes;
    std::vector<std::string> hashedCodes;
    plainCodes.reserve(kBackupCodeCount);
    hashedCodes.reserve(kBackupCodeCount);
    for (int i = 0; i < kBackupCodeCount; ++i) {
        const std::string plainCode = generateBackupCode();
        plainCodes.push_back(plainCode);
        hashedCodes.push_back(password_hash::hash(plainCode));
    }
    repository_.addBackupCodes(login, hashedCodes);

    return ConfirmTotpOutcome{.result = ConfirmTotpResult::kConfirmed, .backupCodes = plainCodes};
}

DisableTotpResult UserService::disableTotp(const std::string& login, const std::string& code) {
    if (!repository_.isTotpEnabled(login)) {
        return DisableTotpResult::kNotEnabled;
    }
    const std::optional<std::string> secretBase64 = repository_.findTotpSecret(login);
    const std::optional<std::string> secret =
        secretBase64.has_value() ? base64::decode(*secretBase64) : std::nullopt;
    if (!secret.has_value() || !totp::verify(*secret, code, nowUnixSeconds())) {
        return DisableTotpResult::kInvalidCode;
    }

    repository_.disableTotp(login);
    return DisableTotpResult::kDisabled;
}

bool UserService::isTotpEnabled(const std::string& login) {
    return repository_.isTotpEnabled(login);
}

bool UserService::verifyTotpOrBackupCode(const std::string& login, const std::string& code) {
    if (!repository_.isTotpEnabled(login)) {
        return false;
    }

    const std::optional<std::string> secretBase64 = repository_.findTotpSecret(login);
    const std::optional<std::string> secret =
        secretBase64.has_value() ? base64::decode(*secretBase64) : std::nullopt;
    if (secret.has_value() && totp::verify(*secret, code, nowUnixSeconds())) {
        return true;
    }

    for (const std::string& hash : repository_.findUnusedBackupCodeHashes(login)) {
        if (password_hash::verify(hash, code)) {
            repository_.markBackupCodeUsed(login, hash);
            return true;
        }
    }
    return false;
}

}  // namespace user_service
