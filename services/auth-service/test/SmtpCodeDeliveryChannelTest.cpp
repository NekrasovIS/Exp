#include "SmtpCodeDeliveryChannel.h"

#include <gtest/gtest.h>

#include <cstdlib>

namespace auth_service {
namespace {

// Переменные окружения — процесс-глобальное состояние, поэтому каждый
// тест сохраняет и восстанавливает то, что трогает, а не полагается на
// порядок выполнения других тестов.
class EnvVarGuard {
public:
    EnvVarGuard(const char* name, const char* value) : name_(name) {
#ifdef _WIN32
        _putenv_s(name_, value);
#else
        setenv(name_, value, /*overwrite=*/1);
#endif
    }
    ~EnvVarGuard() {
#ifdef _WIN32
        _putenv_s(name_, "");
#else
        unsetenv(name_);
#endif
    }
    EnvVarGuard(const EnvVarGuard&) = delete;
    EnvVarGuard& operator=(const EnvVarGuard&) = delete;

private:
    const char* name_;
};

TEST(SmtpCodeDeliveryChannelTest, FromEnvironmentReturnsNulloptWithoutSmtpHost) {
    const EnvVarGuard guard("SMTP_HOST", "");

    EXPECT_FALSE(SmtpCodeDeliveryChannel::fromEnvironment().has_value());
}

TEST(SmtpCodeDeliveryChannelTest, FromEnvironmentParsesAllFieldsWhenSet) {
    const EnvVarGuard host("SMTP_HOST", "smtp.example.test");
    const EnvVarGuard port("SMTP_PORT", "587");
    const EnvVarGuard username("SMTP_USERNAME", "otp-bot");
    const EnvVarGuard password("SMTP_PASSWORD", "super-secret");
    const EnvVarGuard from("SMTP_FROM", "otp@example.test");

    const auto config = SmtpCodeDeliveryChannel::fromEnvironment();

    ASSERT_TRUE(config.has_value());
    EXPECT_EQ(config->host, "smtp.example.test");
    EXPECT_EQ(config->port, 587);
    EXPECT_EQ(config->username, "otp-bot");
    EXPECT_EQ(config->password, "super-secret");
    EXPECT_EQ(config->fromAddress, "otp@example.test");
}

TEST(SmtpCodeDeliveryChannelTest, FromEnvironmentFallsBackToUsernameWhenFromIsUnset) {
    const EnvVarGuard host("SMTP_HOST", "smtp.example.test");
    const EnvVarGuard username("SMTP_USERNAME", "otp-bot@example.test");
    const EnvVarGuard from("SMTP_FROM", "");

    const auto config = SmtpCodeDeliveryChannel::fromEnvironment();

    ASSERT_TRUE(config.has_value());
    EXPECT_EQ(config->fromAddress, "otp-bot@example.test");
}

}  // namespace
}  // namespace auth_service
