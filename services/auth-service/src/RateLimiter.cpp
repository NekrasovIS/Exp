#include "RateLimiter.h"

namespace auth_service {

RateLimiter::RateLimiter(int maxRequestsPerWindow, std::chrono::milliseconds window)
    : maxRequestsPerWindow_(maxRequestsPerWindow), window_(window) {}

bool RateLimiter::allow(const std::string& key) {
    const auto now = std::chrono::steady_clock::now();
    const std::lock_guard<std::mutex> lock(mutex_);
    Window& entry = windows_[key];
    if (now - entry.windowStart >= window_) {
        entry.windowStart = now;
        entry.count = 0;
    }
    if (entry.count >= maxRequestsPerWindow_) {
        return false;
    }
    ++entry.count;
    return true;
}

}  // namespace auth_service
