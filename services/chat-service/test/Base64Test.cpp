#include "base64.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <random>
#include <string>

namespace chat_service::base64 {
namespace {

// Собственные тестовые векторы RFC 4648 §10 — покрывают 0/1/2 символа паддинга.
TEST(Base64Test, DecodesEmptyStringAsEmpty) {
    const std::optional<std::string> decoded = decode("");
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(*decoded, "");
}

TEST(Base64Test, DecodesRfc4648TestVectors) {
    EXPECT_EQ(decode("Zg=="), "f");
    EXPECT_EQ(decode("Zm8="), "fo");
    EXPECT_EQ(decode("Zm9v"), "foo");
    EXPECT_EQ(decode("Zm9vYg=="), "foob");
    EXPECT_EQ(decode("Zm9vYmE="), "fooba");
    EXPECT_EQ(decode("Zm9vYmFy"), "foobar");
}

TEST(Base64Test, DecodesArbitraryBytesIncludingHighAndLowValues) {
    // Вычислено заранее: base64 от байтов {0x00, 0x01, 0x02, 0xFF, 0x7F, 0x80}.
    const std::optional<std::string> decoded = decode("AAEC/3+A");
    ASSERT_TRUE(decoded.has_value());
    ASSERT_EQ(decoded->size(), 6U);
    EXPECT_EQ(static_cast<unsigned char>((*decoded)[0]), 0x00);
    EXPECT_EQ(static_cast<unsigned char>((*decoded)[1]), 0x01);
    EXPECT_EQ(static_cast<unsigned char>((*decoded)[2]), 0x02);
    EXPECT_EQ(static_cast<unsigned char>((*decoded)[3]), 0xFF);
    EXPECT_EQ(static_cast<unsigned char>((*decoded)[4]), 0x7F);
    EXPECT_EQ(static_cast<unsigned char>((*decoded)[5]), 0x80);
}

TEST(Base64Test, IgnoresWhitespaceAndNewlines) {
    EXPECT_EQ(decode("Zm9v\nYmFy"), "foobar");
    EXPECT_EQ(decode("Zm9v YmFy"), "foobar");
}

TEST(Base64Test, RejectsInvalidAlphabetCharacter) {
    EXPECT_FALSE(decode("not!valid$$$base64").has_value());
}

// Временная замена настоящему fuzzing'у, управляемому покрытием (issue #121
// — libFuzzer требует clang-cl, недоступный в тулчейне MSVC этого
// окружения): бросает тысячи случайных байтовых строк — контролируемый
// атакующим `data_base64` из POST /channels/{id}/attachments это именно
// такой недоверенный ввод — в decode() с фиксированным seed для
// воспроизводимости. Единственное проверяемое свойство — что decode()
// никогда не падает и не приводит к UB на произвольных байтах, включая
// встроенный NUL и весь диапазон 0-255; возврат valid-или-nullopt в
// любом случае корректен, так что больше проверять нечего.
TEST(Base64Test, DoesNotCrashOnRandomByteStrings) {
    std::mt19937 rng(0xB4501234U);
    std::uniform_int_distribution<int> lengthDist(0, 256);
    std::uniform_int_distribution<int> byteDist(0, 255);

    for (int iteration = 0; iteration < 5000; ++iteration) {
        const int length = lengthDist(rng);
        std::string input;
        input.reserve(static_cast<std::size_t>(length));
        for (int i = 0; i < length; ++i) {
            input.push_back(static_cast<char>(static_cast<std::uint8_t>(byteDist(rng))));
        }
        // Результат не проверяется — decode() может принять или отвергнуть
        // любую данную случайную строку; тест — просто пережить это без падения.
        static_cast<void>(decode(input));
    }
}

}  // namespace
}  // namespace chat_service::base64
