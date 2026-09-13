#include "totp.h"

#include <gtest/gtest.h>

namespace user_service::totp {
namespace {

// RFC 6238 Appendix B's own published test vectors — SHA1, 8-digit
// codes, the ASCII string "12345678901234567890" used directly as the
// HMAC key (not base32-decoded — that's how the RFC's own reference
// vectors are defined). Digits is 8 here specifically to match these
// published values exactly, proving the HMAC + dynamic-truncation
// logic is correct against an external reference; the product's own
// default (6 digits, see totp.h) is exercised separately below.
constexpr const char* kRfcKey = "12345678901234567890";

TEST(TotpTest, MatchesRfc6238AppendixBTestVectors) {
    EXPECT_EQ(totp(kRfcKey, 59, 8), "94287082");
    EXPECT_EQ(totp(kRfcKey, 1111111109, 8), "07081804");
    EXPECT_EQ(totp(kRfcKey, 1111111111, 8), "14050471");
    EXPECT_EQ(totp(kRfcKey, 1234567890, 8), "89005924");
    EXPECT_EQ(totp(kRfcKey, 2000000000, 8), "69279037");
}

TEST(TotpTest, DefaultDigitsIsSixAndZeroPadded) {
    // Same key/time as the first RFC vector above (94287082) — the
    // 6-digit code is simply that same dynamic-truncation value modulo
    // 10^6, i.e. its last 6 digits: "287082".
    EXPECT_EQ(totp(kRfcKey, 59), "287082");
}

TEST(TotpTest, HotpProducesZeroPaddedCodes) {
    // Counter 0 with an arbitrary short key — only asserting the
    // zero-padding/length invariant here, not a specific published
    // vector (RFC 4226's own vectors use a 20-byte key and print all 10
    // counters — redundant to re-derive when TotpTest above already
    // proves the shared HMAC+truncation core against RFC 6238).
    const std::string code = hotp("some-secret-key-bytes", 0);
    EXPECT_EQ(code.size(), 6U);
    for (const char c : code) {
        EXPECT_TRUE(c >= '0' && c <= '9');
    }
}

TEST(TotpTest, VerifyAcceptsTheCurrentStepAndRejectsAWrongCode) {
    const std::string secret = generateSecret();
    const int64_t now = 1700000000;

    EXPECT_TRUE(verify(secret, totp(secret, now), now));
    EXPECT_FALSE(verify(secret, "000000", now));
}

TEST(TotpTest, VerifyToleratesOneStepOfClockDriftInEitherDirection) {
    const std::string secret = generateSecret();
    const int64_t now = 1700000000;
    const std::string codeOneStepAgo = totp(secret, now - 30);
    const std::string codeOneStepAhead = totp(secret, now + 30);

    EXPECT_TRUE(verify(secret, codeOneStepAgo, now, /*windowSteps=*/1));
    EXPECT_TRUE(verify(secret, codeOneStepAhead, now, /*windowSteps=*/1));
}

TEST(TotpTest, VerifyRejectsCodesOutsideTheWindow) {
    const std::string secret = generateSecret();
    const int64_t now = 1700000000;
    const std::string codeTwoStepsAgo = totp(secret, now - 60);

    EXPECT_FALSE(verify(secret, codeTwoStepsAgo, now, /*windowSteps=*/1));
}

TEST(TotpTest, GenerateSecretProducesTheRequestedLengthAndIsNotConstant) {
    const std::string first = generateSecret();
    const std::string second = generateSecret();

    EXPECT_EQ(first.size(), 20U);
    EXPECT_NE(first, second);
}

}  // namespace
}  // namespace user_service::totp
