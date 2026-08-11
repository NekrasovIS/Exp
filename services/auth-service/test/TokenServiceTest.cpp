#include "TokenService.h"

#include <gtest/gtest.h>
#include <openssl/hmac.h>

#include <chrono>
#include <vector>

#include "base64_utils.h"

namespace auth_service {
namespace {

TEST(TokenServiceTest, IssuedTokenVerifiesWithSameSubject) {
    const TokenService service("test-secret");
    const Token token = service.issueToken("alice");

    const std::optional<std::string> subject = service.verifyToken(token.value);

    ASSERT_TRUE(subject.has_value());
    EXPECT_EQ(*subject, "alice");
}

TEST(TokenServiceTest, TokenSignedWithDifferentSecretIsRejected) {
    const TokenService issuer("secret-a");
    const TokenService verifier("secret-b");
    const Token token = issuer.issueToken("alice");

    EXPECT_FALSE(verifier.verifyToken(token.value).has_value());
}

TEST(TokenServiceTest, TamperedPayloadIsRejected) {
    const TokenService service("test-secret");
    const Token token = service.issueToken("alice");

    const auto separator = token.value.find('.');
    ASSERT_NE(separator, std::string::npos);
    std::string tampered = token.value;
    tampered[0] = (tampered[0] == 'A') ? 'B' : 'A';

    EXPECT_FALSE(service.verifyToken(tampered).has_value());
}

TEST(TokenServiceTest, MalformedTokenWithoutSeparatorIsRejected) {
    const TokenService service("test-secret");
    EXPECT_FALSE(service.verifyToken("not-a-valid-token").has_value());
}

TEST(TokenServiceTest, ExpiredTokenIsRejected) {
    const TokenService service("test-secret", std::chrono::seconds{0});
    const Token token = service.issueToken("alice");

    EXPECT_FALSE(service.verifyToken(token.value).has_value());
}

TEST(TokenServiceTest, ValidSignatureButPayloadIsNotValidJsonIsRejected) {
    // Correctly signed (same secret + real HMAC), but the payload
    // segment doesn't decode to valid JSON — covers the parse-failure
    // branch in verifyToken() distinct from a signature mismatch.
    const std::string secret = "test-secret";
    const TokenService service(secret);

    const std::string payloadJson = "not actually json";
    const std::string payloadB64 =
        base64_utils::encodeUrl(std::vector<uint8_t>(payloadJson.begin(), payloadJson.end()));

    std::vector<uint8_t> digest(EVP_MAX_MD_SIZE);
    unsigned int digestLen = 0;
    HMAC(EVP_sha256(), secret.data(), static_cast<int>(secret.size()),
         reinterpret_cast<const unsigned char*>(payloadB64.data()), payloadB64.size(), digest.data(), &digestLen);
    digest.resize(digestLen);
    const std::string signatureB64 = base64_utils::encodeUrl(digest);

    EXPECT_FALSE(service.verifyToken(payloadB64 + "." + signatureB64).has_value());
}

}  // namespace
}  // namespace auth_service
