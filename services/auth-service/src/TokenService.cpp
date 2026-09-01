#include "TokenService.h"

#include <openssl/crypto.h>
#include <openssl/hmac.h>

#include <nlohmann/json.hpp>

#include "JsonGuard.h"
#include "base64_utils.h"

namespace auth_service {

namespace {

std::vector<uint8_t> hmacSha256(const std::string& key, const std::string& message) {
    std::vector<uint8_t> digest(EVP_MAX_MD_SIZE);
    unsigned int digestLen = 0;

    HMAC(EVP_sha256(), key.data(), static_cast<int>(key.size()),
         reinterpret_cast<const unsigned char*>(message.data()), message.size(), digest.data(), &digestLen);

    digest.resize(digestLen);
    return digest;
}

std::vector<uint8_t> toBytes(const std::string& text) {
    return {text.begin(), text.end()};
}

}  // namespace

TokenService::TokenService(std::string secret, std::chrono::seconds ttl, std::chrono::seconds refreshTtl)
    : secret_(std::move(secret)), ttl_(ttl), refreshTtl_(refreshTtl) {}

Token TokenService::issueTokenInternal(const std::string& subject, std::chrono::seconds ttl, bool isRefresh) const {
    const auto expiresAt =
        std::chrono::duration_cast<std::chrono::seconds>((std::chrono::system_clock::now() + ttl).time_since_epoch())
            .count();

    nlohmann::json payload{{"sub", subject}, {"exp", expiresAt}};
    if (isRefresh) {
        payload["typ"] = "refresh";
    }
    const std::string payloadB64 = base64_utils::encodeUrl(toBytes(payload.dump()));
    const std::string signatureB64 = base64_utils::encodeUrl(hmacSha256(secret_, payloadB64));

    return Token{.value = payloadB64 + "." + signatureB64, .expiresAt = expiresAt};
}

std::optional<std::string> TokenService::verifyTokenInternal(const std::string& token, bool expectRefresh) const {
    const auto separator = token.find('.');
    if (separator == std::string::npos) {
        return std::nullopt;
    }

    const std::string payloadB64 = token.substr(0, separator);
    const std::string signatureB64 = token.substr(separator + 1);

    const std::vector<uint8_t> expectedSignature = hmacSha256(secret_, payloadB64);
    const std::vector<uint8_t> providedSignature = base64_utils::decodeUrl(signatureB64);

    if (providedSignature.size() != expectedSignature.size() ||
        CRYPTO_memcmp(providedSignature.data(), expectedSignature.data(), expectedSignature.size()) != 0) {
        return std::nullopt;
    }

    const std::vector<uint8_t> payloadBytes = base64_utils::decodeUrl(payloadB64);
    const std::string_view payloadView(reinterpret_cast<const char*>(payloadBytes.data()), payloadBytes.size());
    if (json_guard::exceedsMaxNestingDepth(payloadView, json_guard::kMaxNestingDepth)) {
        return std::nullopt;
    }
    const nlohmann::json payload =
        nlohmann::json::parse(payloadBytes.begin(), payloadBytes.end(), nullptr, /*allow_exceptions=*/false);
    if (payload.is_discarded() || !payload.contains("sub") || !payload.contains("exp")) {
        return std::nullopt;
    }

    const bool isRefresh = payload.contains("typ") && payload["typ"] == "refresh";
    if (isRefresh != expectRefresh) {
        // Access-токен предъявлен там, где ожидался refresh-токен, или
        // наоборот — отклоняем, а не принимаем молча ни один из
        // вариантов, даже несмотря на то, что оба подписаны одним и тем
        // же секретом.
        return std::nullopt;
    }

    const auto expiresAt = payload["exp"].get<std::int64_t>();
    const auto now =
        std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    if (now >= expiresAt) {
        return std::nullopt;
    }

    return payload["sub"].get<std::string>();
}

Token TokenService::issueToken(const std::string& subject) const {
    return issueTokenInternal(subject, ttl_, /*isRefresh=*/false);
}

std::optional<std::string> TokenService::verifyToken(const std::string& token) const {
    return verifyTokenInternal(token, /*expectRefresh=*/false);
}

Token TokenService::issueRefreshToken(const std::string& subject) const {
    return issueTokenInternal(subject, refreshTtl_, /*isRefresh=*/true);
}

std::optional<std::string> TokenService::verifyRefreshToken(const std::string& token) const {
    return verifyTokenInternal(token, /*expectRefresh=*/true);
}

}  // namespace auth_service
