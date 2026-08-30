#pragma once

#include <chrono>
#include <mutex>
#include <string>
#include <unordered_map>

namespace auth_service {

/**
 * @brief Fixed-window request rate limiter, keyed by an arbitrary
 *        string (in practice, the client's remote address) — issue
 *        #102, applied to auth-service's credential-checking routes
 *        (/auth/token, /auth/register) to slow down brute-force
 *        guessing and account-creation spam.
 *
 * Thread-safe: HttpServer may dispatch onto multiple threads (same as
 * TokenService/UserServiceClient). First-pass tradeoff, not solved
 * here: windows_ grows by one entry per distinct key ever seen and is
 * never evicted, so a long-running process accumulates memory
 * proportional to the number of distinct client addresses it's ever
 * seen — acceptable for now, revisit if this ever actually matters.
 */
class RateLimiter {
public:
    /// @p window as milliseconds (not seconds) so tests can use a
    /// sub-second window and stay fast/deterministic rather than
    /// sleeping a real second to observe a window reset.
    RateLimiter(int maxRequestsPerWindow, std::chrono::milliseconds window);

    /// True if @p key is still under its limit for the current window
    /// (and counts this call towards it); false if it's already at the
    /// limit — the caller should reject the request (e.g. HTTP 429).
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
