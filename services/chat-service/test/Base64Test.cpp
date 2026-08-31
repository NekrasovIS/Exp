#include "base64.h"

#include <gtest/gtest.h>

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

}  // namespace
}  // namespace chat_service::base64
