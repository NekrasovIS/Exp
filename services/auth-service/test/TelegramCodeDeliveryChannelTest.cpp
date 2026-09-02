#include "TelegramCodeDeliveryChannel.h"

#include <gtest/gtest.h>

#include <cstdlib>

namespace auth_service {
namespace {

// Переменные окружения — процесс-глобальное состояние, поэтому каждый
// тест сохраняет и восстанавливает то, что трогает, а не полагается на
// порядок выполнения других тестов (тот же паттерн, что и
// SmtpCodeDeliveryChannelTest.cpp).
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

TEST(TelegramCodeDeliveryChannelTest, FromEnvironmentReturnsNulloptWithoutBotToken) {
    const EnvVarGuard guard("TELEGRAM_BOT_TOKEN", "");

    EXPECT_FALSE(TelegramCodeDeliveryChannel::fromEnvironment().has_value());
}

TEST(TelegramCodeDeliveryChannelTest, FromEnvironmentParsesBotTokenWhenSet) {
    const EnvVarGuard token("TELEGRAM_BOT_TOKEN", "123456:abc-def");

    const auto config = TelegramCodeDeliveryChannel::fromEnvironment();

    ASSERT_TRUE(config.has_value());
    EXPECT_EQ(config->botToken, "123456:abc-def");
}

}  // namespace
}  // namespace auth_service
