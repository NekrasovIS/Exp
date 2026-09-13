#include "base32.h"

#include <gtest/gtest.h>

namespace user_service::base32 {
namespace {

// RFC 4648 §10 test vectors.
TEST(Base32Test, EncodesRfc4648TestVectors) {
    EXPECT_EQ(encode(""), "");
    EXPECT_EQ(encode("f"), "MY======");
    EXPECT_EQ(encode("fo"), "MZXQ====");
    EXPECT_EQ(encode("foo"), "MZXW6===");
    EXPECT_EQ(encode("foob"), "MZXW6YQ=");
    EXPECT_EQ(encode("fooba"), "MZXW6YTB");
    EXPECT_EQ(encode("foobar"), "MZXW6YTBOI======");
}

TEST(Base32Test, OutputUsesOnlyTheDocumentedAlphabetAndPadding) {
    const std::string encoded = encode("some arbitrary TOTP-secret-like bytes \x01\x02\xff");
    for (const char c : encoded) {
        const bool isUpperLetter = c >= 'A' && c <= 'Z';
        const bool isDigit2to7 = c >= '2' && c <= '7';
        EXPECT_TRUE(isUpperLetter || isDigit2to7 || c == '=') << "unexpected character: " << c;
    }
}

TEST(Base32Test, DecodesRfc4648TestVectors) {
    EXPECT_EQ(decode("MY======"), "f");
    EXPECT_EQ(decode("MZXQ===="), "fo");
    EXPECT_EQ(decode("MZXW6==="), "foo");
    EXPECT_EQ(decode("MZXW6YQ="), "foob");
    EXPECT_EQ(decode("MZXW6YTB"), "fooba");
    EXPECT_EQ(decode("MZXW6YTBOI======"), "foobar");
}

TEST(Base32Test, EncodeThenDecodeRoundTripsArbitraryBytes) {
    const std::string original("\x00\x01\x02\xff\x7f\x80TOTP secret bytes", 19);
    const std::optional<std::string> decoded = decode(encode(original));
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(*decoded, original);
}

TEST(Base32Test, RejectsInvalidAlphabetCharacter) {
    EXPECT_FALSE(decode("not-valid-base32!!!").has_value());
}

}  // namespace
}  // namespace user_service::base32
