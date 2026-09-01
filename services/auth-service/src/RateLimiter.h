#pragma once

#include <chrono>
#include <mutex>
#include <string>
#include <unordered_map>

namespace auth_service {

/**
 * @brief Ограничитель частоты запросов с фиксированным окном, ключом
 *        служит произвольная строка (на практике — адрес клиента) —
 *        issue #102, применяется к маршрутам auth-service, проверяющим
 *        учётные данные (/auth/token, /auth/register), чтобы замедлить
 *        подбор паролей методом brute-force и спам созданием аккаунтов.
 *
 * Потокобезопасен: HttpServer может диспетчеризовать запросы на
 * несколько потоков (как и TokenService/UserServiceClient). Компромисс
 * первого прохода, не решённый здесь: windows_ растёт на одну запись
 * на каждый когда-либо встреченный уникальный ключ и никогда не
 * очищается, поэтому долго работающий процесс накапливает память
 * пропорционально числу когда-либо виденных уникальных клиентских
 * адресов — приемлемо на данный момент, вернуться к этому, если это
 * действительно станет проблемой.
 */
class RateLimiter {
public:
    /// @p window задаётся в миллисекундах (а не в секундах), чтобы тесты
    /// могли использовать окно короче секунды и оставаться быстрыми и
    /// детерминированными, а не ждать реальную секунду, чтобы увидеть
    /// сброс окна.
    RateLimiter(int maxRequestsPerWindow, std::chrono::milliseconds window);

    /// True, если @p key всё ещё не превысил лимит для текущего окна
    /// (и этот вызов засчитывается в счётчик); false, если лимит уже
    /// исчерпан — вызывающий код должен отклонить запрос (например,
    /// вернуть HTTP 429).
    [[nodiscard]] bool allow(const std::string& key);

private:
    struct Window {
        std::chrono::steady_clock::time_point windowStart{};
        int count = 0;
    };

    int maxRequestsPerWindow_;
    std::chrono::milliseconds window_;
    std::mutex mutex_;
    std::unordered_map<std::string, Window> windows_;
};

}  // namespace auth_service
