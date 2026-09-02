#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>

namespace auth_service {

/// Только что выданный токен и момент его истечения (Unix-время в секундах, UTC).
struct Token {
    std::string value;
    std::int64_t expiresAt = 0;
};

/**
 * @brief Выдаёт и проверяет подписанные HMAC-SHA256 токены с
 *        ограниченным сроком действия — как короткоживущие
 *        access-токены, так и долгоживущие refresh-токены (issue #105).
 *
 * Формат токена: base64url(json-полезная нагрузка) + "." +
 * base64url(HMAC-SHA256 от сегмента полезной нагрузки). Намеренно не
 * полноценный JWT (нет сегмента заголовка, нет согласования алгоритма)
 * — этот сервис только производит и потребляет собственные токены,
 * поэтому дополнительная машинерия JWT ничего не даёт.
 *
 * Refresh-токен имеет тот же формат, плюс `"typ": "refresh"` в полезной
 * нагрузке; полезная нагрузка access-токена никогда не содержит этот
 * ключ. verifyToken() требует отсутствия этого ключа, verifyRefreshToken()
 * требует его наличия, поэтому refresh-токен никогда не может быть
 * использован вместо access-токена (и наоборот), даже несмотря на то,
 * что оба подписаны одним и тем же секретом. Refresh-токены не
 * ротируются — обмен одного из них через verifyRefreshToken() +
 * issueToken() просто выпускает свежий access-токен, а сам refresh-токен
 * продолжает работать до истечения своего срока.
 *
 * Чистая логика, без сетевого взаимодействия: тестируется изолированно
 * от HttpServer.
 */
class TokenService {
public:
    explicit TokenService(std::string secret, std::chrono::seconds ttl = std::chrono::seconds{3600},
                           std::chrono::seconds refreshTtl = std::chrono::seconds{30 * 24 * 3600});

    /// Выдаёт новый access-токен для @p subject, действительный в
    /// течение (короткого) TTL этого сервиса.
    [[nodiscard]] Token issueToken(const std::string& subject) const;

    /// @return Субъект access-токена, если у @p token валидная подпись,
    ///         он не истёк и не является на самом деле refresh-токеном;
    ///         иначе std::nullopt.
    [[nodiscard]] std::optional<std::string> verifyToken(const std::string& token) const;

    /// Выдаёт новый refresh-токен для @p subject, действительный в
    /// течение (долгого) refresh-TTL этого сервиса — обменивается через
    /// verifyRefreshToken() на свежие access-токены без повторного ввода
    /// учётных данных пользователем.
    [[nodiscard]] Token issueRefreshToken(const std::string& subject) const;

    /// @return Субъект @p token, если это валидный, не истёкший
    ///         refresh-токен; иначе std::nullopt — в том числе если
    ///         передан обычный access-токен, который здесь отклоняется,
    ///         а не принимается молча.
    [[nodiscard]] std::optional<std::string> verifyRefreshToken(const std::string& token) const;

private:
    [[nodiscard]] Token issueTokenInternal(const std::string& subject, std::chrono::seconds ttl,
                                            bool isRefresh) const;
    [[nodiscard]] std::optional<std::string> verifyTokenInternal(const std::string& token, bool expectRefresh) const;

    std::string secret_;
    std::chrono::seconds ttl_;
    std::chrono::seconds refreshTtl_;
};

}  // namespace auth_service
