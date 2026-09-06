#include "base64_utils.h"

#include <gtest/gtest.h>

namespace base64_utils {
namespace {

TEST(Base64UtilsTest, EncodesEmptyDataAsEmptyString) {
    EXPECT_EQ(encodeUrl({}), "");
}

TEST(Base64UtilsTest, RoundTripsArbitraryBytes) {
    const std::vector<uint8_t> data{0x00, 0x01, 0x02, 0xFF, 0x7F, 0x80, 'h', 'i'};

    EXPECT_EQ(decodeUrl(encodeUrl(data)), data);
}

TEST(Base64UtilsTest, RoundTripsOneRemainderByte) {
    const std::vector<uint8_t> data{0xAB};

    EXPECT_EQ(decodeUrl(encodeUrl(data)), data);
}

TEST(Base64UtilsTest, RoundTripsTwoRemainderBytes) {
    const std::vector<uint8_t> data{0xAB, 0xCD};

    EXPECT_EQ(decodeUrl(encodeUrl(data)), data);
}

TEST(Base64UtilsTest, EncodedOutputUsesUrlSafeAlphabetOnly) {
    // Байты подобраны так, что стандартный base64 выдал бы '+'/'/', если
    // бы использовался не URL-safe алфавит.
    const std::vector<uint8_t> data{0xFB, 0xFF, 0xBF};

    const std::string encoded = encodeUrl(data);

    EXPECT_EQ(encoded.find('+'), std::string::npos);
    EXPECT_EQ(encoded.find('/'), std::string::npos);
    EXPECT_EQ(encoded.find('='), std::string::npos);
}

TEST(Base64UtilsTest, DecodeRejectsInvalidAlphabetCharacter) {
    EXPECT_TRUE(decodeUrl("not!valid+base64").empty());
}

TEST(Base64UtilsTest, DecodeOfEmptyStringIsEmptyVector) {
    EXPECT_TRUE(decodeUrl("").empty());
}

}  // namespace
}  // namespace base64_utils
