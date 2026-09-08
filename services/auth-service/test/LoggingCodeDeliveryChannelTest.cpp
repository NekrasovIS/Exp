#include "LoggingCodeDeliveryChannel.h"

#include <gtest/gtest.h>

#include <iostream>
#include <sstream>

namespace auth_service {
namespace {

// Issue #227 (покрытие тестами) — раньше 0%, хотя это не сетевой код
// (в отличие от Smtp/TelegramCodeDeliveryChannel), а просто вывод в
// stdout, легко проверяемый подменой буфера std::cout.
TEST(LoggingCodeDeliveryChannelTest, SendWritesDestinationAndCodeToStdout) {
    std::ostringstream captured;
    std::streambuf* const originalBuffer = std::cout.rdbuf(captured.rdbuf());

    const LoggingCodeDeliveryChannel channel;
    channel.send("alice@example.com", "123456");

    std::cout.rdbuf(originalBuffer);

    const std::string output = captured.str();
    EXPECT_NE(output.find("alice@example.com"), std::string::npos);
    EXPECT_NE(output.find("123456"), std::string::npos);
}

}  // namespace
}  // namespace auth_service
