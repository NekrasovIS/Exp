#include "UserRepository.h"
#include "UserService.h"
#include "base32.h"
#include "totp.h"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <stdexcept>
#include <string>

// Требует работающий Postgres (см. docker-compose.yml), доступный по
// USER_SERVICE_DATABASE_URL (значение по умолчанию совпадает с docker-compose.yml).
// Пропускает себя вместо падения, если он не запущен.

namespace user_service {
namespace {

std::string envOrDefault(const char* name, const std::string& defaultValue) {
    const char* value = std::getenv(name);
    return value != nullptr ? std::string(value) : defaultValue;
}

TEST(UserServiceIntegrationTest, RegisterThenVerifyCredentialsRoundTrip) {
    const std::string connectionString = envOrDefault(
        "USER_SERVICE_DATABASE_URL", "postgresql://user_service:dev-only-password@localhost:5433/user_service");

    UserRepository repository(connectionString);
    UserService service(repository);

    const std::string login =
        "user-service-integration-test-" +
        std::to_string(
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
                .count());
    const std::string password = "integration-test-password";

    bool registered = false;
    try {
        registered = service.registerUser(login, password);
    } catch (const std::exception& error) {
        GTEST_SKIP() << "Postgres not reachable (" << error.what() << ") — run `docker compose up` to run this test.";
    }

    ASSERT_TRUE(registered);
    EXPECT_TRUE(service.verifyCredentials(login, password));
    EXPECT_FALSE(service.verifyCredentials(login, "wrong-password"));
    EXPECT_FALSE(service.verifyCredentials("no-such-user", password));
}

TEST(UserServiceIntegrationTest, RegisterUserRejectsDuplicateLogin) {
    const std::string connectionString = envOrDefault(
        "USER_SERVICE_DATABASE_URL", "postgresql://user_service:dev-only-password@localhost:5433/user_service");

    UserRepository repository(connectionString);
    UserService service(repository);

    const std::string login =
        "user-service-dup-test-" +
        std::to_string(
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
                .count());
    const std::string password = "integration-test-password";

    bool firstRegistered = false;
    try {
        firstRegistered = service.registerUser(login, password);
    } catch (const std::exception& error) {
        GTEST_SKIP() << "Postgres not reachable (" << error.what() << ") — run `docker compose up` to run this test.";
    }

    ASSERT_TRUE(firstRegistered);
    EXPECT_FALSE(service.registerUser(login, "a-different-password"));
}

std::int64_t nowUnixSeconds() {
    return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch())
        .count();
}

TEST(UserServiceIntegrationTest, TotpSetupConfirmVerifyAndDisableRoundTrip) {
    const std::string connectionString = envOrDefault(
        "USER_SERVICE_DATABASE_URL", "postgresql://user_service:dev-only-password@localhost:5433/user_service");

    UserRepository repository(connectionString);
    UserService service(repository);

    const std::string login =
        "user-service-totp-test-" +
        std::to_string(
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
                .count());

    bool registered = false;
    try {
        registered = service.registerUser(login, "irrelevant-password");
    } catch (const std::exception& error) {
        GTEST_SKIP() << "Postgres not reachable (" << error.what() << ") — run `docker compose up` to run this test.";
    }
    ASSERT_TRUE(registered);

    EXPECT_FALSE(service.isTotpEnabled(login));
    // auth-service would call this mid-login — with TOTP not yet even
    // set up, it must fail closed, not throw/error.
    EXPECT_FALSE(service.verifyTotpOrBackupCode(login, "000000"));

    const std::optional<TotpSetupInfo> setup = service.setupTotp(login);
    ASSERT_TRUE(setup.has_value());
    EXPECT_FALSE(setup->secretBase32.empty());
    EXPECT_NE(setup->otpauthUrl.find("otpauth://totp/DeviceHub:" + login), std::string::npos);
    EXPECT_FALSE(service.isTotpEnabled(login)) << "not enabled until confirmed";

    const std::optional<std::string> rawSecret = base32::decode(setup->secretBase32);
    ASSERT_TRUE(rawSecret.has_value());
    const std::string currentCode = totp::totp(*rawSecret, nowUnixSeconds());

    // issue #388: a wrong code doesn't confirm.
    const ConfirmTotpOutcome wrongOutcome = service.confirmTotp(login, "000000");
    EXPECT_EQ(wrongOutcome.result, ConfirmTotpResult::kInvalidCode);
    EXPECT_FALSE(service.isTotpEnabled(login));

    const ConfirmTotpOutcome outcome = service.confirmTotp(login, currentCode);
    ASSERT_EQ(outcome.result, ConfirmTotpResult::kConfirmed);
    EXPECT_EQ(outcome.backupCodes.size(), 10U);
    EXPECT_TRUE(service.isTotpEnabled(login));

    // The same current code verifies a login attempt...
    EXPECT_TRUE(service.verifyTotpOrBackupCode(login, currentCode));
    // ...and so does a backup code, used exactly once.
    const std::string backupCode = outcome.backupCodes.front();
    EXPECT_TRUE(service.verifyTotpOrBackupCode(login, backupCode));
    EXPECT_FALSE(service.verifyTotpOrBackupCode(login, backupCode)) << "a backup code is single-use";
    EXPECT_FALSE(service.verifyTotpOrBackupCode(login, "not-a-real-code"));

    // Disabling requires a currently-valid code, not just any caller.
    EXPECT_EQ(service.disableTotp(login, "000000"), DisableTotpResult::kInvalidCode);
    EXPECT_TRUE(service.isTotpEnabled(login));
    EXPECT_EQ(service.disableTotp(login, totp::totp(*rawSecret, nowUnixSeconds())), DisableTotpResult::kDisabled);
    EXPECT_FALSE(service.isTotpEnabled(login));
    EXPECT_FALSE(service.verifyTotpOrBackupCode(login, currentCode));
}

TEST(UserServiceIntegrationTest, SetupTotpFailsWhileAlreadyEnabled) {
    const std::string connectionString = envOrDefault(
        "USER_SERVICE_DATABASE_URL", "postgresql://user_service:dev-only-password@localhost:5433/user_service");

    UserRepository repository(connectionString);
    UserService service(repository);

    const std::string login =
        "user-service-totp-reenable-test-" +
        std::to_string(
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
                .count());

    bool registered = false;
    try {
        registered = service.registerUser(login, "irrelevant-password");
    } catch (const std::exception& error) {
        GTEST_SKIP() << "Postgres not reachable (" << error.what() << ") — run `docker compose up` to run this test.";
    }
    ASSERT_TRUE(registered);

    const std::optional<TotpSetupInfo> setup = service.setupTotp(login);
    ASSERT_TRUE(setup.has_value());
    const std::optional<std::string> rawSecret = base32::decode(setup->secretBase32);
    ASSERT_TRUE(rawSecret.has_value());
    ASSERT_EQ(service.confirmTotp(login, totp::totp(*rawSecret, nowUnixSeconds())).result,
              ConfirmTotpResult::kConfirmed);

    EXPECT_FALSE(service.setupTotp(login).has_value());
}

}  // namespace
}  // namespace user_service
