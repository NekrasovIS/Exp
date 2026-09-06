#pragma once

#include <chrono>
#include <mutex>
#include <string>
#include <unordered_map>

namespace auth_service {

/**
 * @brief Одноразовые коды в памяти, по одному на login, для входа по
 *        коду (issue #156) — тот же подход, что и у RateLimiter
 *        (in-memory, под мьютексом): у auth-service нет собственной БД.
 *
 * Коды хранятся хешированными (SHA-256 через EVP-интерфейс OpenSSL),
 * никогда в открытом виде, и истекают по TTL. Неверная попытка не
 * аннулирует код сразу, а учитывается в счётчике попыток — так
 * 6-значный код нельзя перебрать в пределах его времени жизни, даже
 * если атакующий обходит RateLimiter по IP (например, меняя адреса).
 */
class OtpStore {
public:
    OtpStore(std::chrono::seconds ttl, int maxAttempts);

    /// Генерирует новый 6-значный код для @p login (заменяя любой ещё
    /// не использованный код для него), сохраняет его хеш и
    /// возвращает код в открытом виде для доставки вызывающей стороной.
    [[nodiscard]] std::string issue(const std::string& login);

    /// @return true, если @p code совпадает с ещё действующим кодом для
    /// @p login — одноразовый: при успехе запись удаляется. Неверный
    /// код учитывается в счётчике попыток; когда они заканчиваются
    /// (или код истекает), запись удаляется и метод возвращает false,
    /// пока не будет вызван issue() заново.
    [[nodiscard]] bool verify(const std::string& login, const std::string& code);

private:
    struct Entry {
        std::string codeHash;
        std::chrono::steady_clock::time_point expiresAt;
        int attemptsRemaining;
    };

    [[nodiscard]] static std::string generateNumericCode();
    [[nodiscard]] static std::string hashCode(const std::string& code);

    std::chrono::seconds ttl_;
    int maxAttempts_;
    std::mutex mutex_;
    std::unordered_map<std::string, Entry> entries_;
};

}  // namespace auth_service
