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
    // Корректно подписан (тот же секрет + настоящий HMAC), но сегмент
    // полезной нагрузки не декодируется в валидный JSON — покрывает
    // ветку ошибки разбора в verifyToken(), отличную от несовпадения подписи.
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

TEST(TokenServiceTest, IssuedRefreshTokenVerifiesWithSameSubject) {
    const TokenService service("test-secret");
    const Token refreshToken = service.issueRefreshToken("alice");

    const std::optional<std::string> subject = service.verifyRefreshToken(refreshToken.value);

    ASSERT_TRUE(subject.has_value());
    EXPECT_EQ(*subject, "alice");
}

TEST(TokenServiceTest, AccessTokenIsRejectedAsARefreshToken) {
    const TokenService service("test-secret");
    const Token accessToken = service.issueToken("alice");

    EXPECT_FALSE(service.verifyRefreshToken(accessToken.value).has_value());
}

TEST(TokenServiceTest, RefreshTokenIsRejectedAsAnAccessToken) {
    const TokenService service("test-secret");
    const Token refreshToken = service.issueRefreshToken("alice");

    EXPECT_FALSE(service.verifyToken(refreshToken.value).has_value());
}

TEST(TokenServiceTest, PayloadSplicedWithAnotherUsersSignatureIsRejected) {
    // issue #224 (pentest): "склеенный" токен — payload одного
    // пользователя + подпись от токена другого — не должен пройти
    // проверку, даже несмотря на то, что оба подписаны одним и тем же
    // секретом. HMAC покрывает конкретный payload целиком, так что
    // подпись, снятая с payload'а B, не совпадёт с ожидаемой подписью
    // payload'а A.
    const TokenService service("test-secret");
    const Token tokenAlice = service.issueToken("alice");
    const Token tokenBob = service.issueToken("bob");

    const auto aliceSeparator = tokenAlice.value.find('.');
    const auto bobSeparator = tokenBob.value.find('.');
    ASSERT_NE(aliceSeparator, std::string::npos);
    ASSERT_NE(bobSeparator, std::string::npos);
    const std::string splicedToken = tokenAlice.value.substr(0, aliceSeparator) + tokenBob.value.substr(bobSeparator);

    EXPECT_FALSE(service.verifyToken(splicedToken).has_value());
}

TEST(TokenServiceTest, ExpiredRefreshTokenIsRejected) {
    const TokenService service("test-secret", std::chrono::seconds{3600}, std::chrono::seconds{0});
    const Token refreshToken = service.issueRefreshToken("alice");

    EXPECT_FALSE(service.verifyRefreshToken(refreshToken.value).has_value());
}

}  // namespace
}  // namespace auth_service
