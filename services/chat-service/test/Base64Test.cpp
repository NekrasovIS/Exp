#include "base64.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <random>
#include <string>

namespace chat_service::base64 {
namespace {

// RFC 4648 §10's own test vectors — cover 0/1/2 padding characters.
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
    // Precomputed: base64 of bytes {0x00, 0x01, 0x02, 0xFF, 0x7F, 0x80}.
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

// Interim stand-in for real coverage-guided fuzzing (issue #121 — libFuzzer
// needs clang-cl, unavailable in this environment's MSVC toolchain): throws
// thousands of random byte strings — attacker-controlled `data_base64` from
// POST /channels/{id}/attachments is exactly this kind of untrusted input —
// at decode() with a fixed seed for reproducibility. The only property
// checked is that decode() never crashes/UB's on arbitrary bytes, including
// embedded NUL and the full 0-255 range; a valid-or-nullopt return is
// correct either way, so there's nothing else to assert against.
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
        // Not asserting on the result — decode() may accept or reject any
        // given random string; surviving without crashing is the test.
        static_cast<void>(decode(input));
    }
}

}  // namespace
}  // namespace chat_service::base64
