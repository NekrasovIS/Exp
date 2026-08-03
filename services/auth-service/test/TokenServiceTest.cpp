#include "TokenService.h"

#include <gtest/gtest.h>

#include <chrono>

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

}  // namespace
}  // namespace auth_service
