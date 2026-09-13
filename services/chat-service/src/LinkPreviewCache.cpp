#include "LinkPreviewCache.h"

namespace chat_service {

LinkPreviewCache::LinkPreviewCache(std::chrono::seconds ttl) : ttl_(ttl) {}

std::optional<LinkPreviewResult> LinkPreviewCache::get(const std::string& url) const {
    const std::lock_guard<std::mutex> lock(mutex_);
    const auto it = entries_.find(url);
    if (it == entries_.end() || std::chrono::steady_clock::now() >= it->second.expiresAt) {
        return std::nullopt;
    }
    return it->second.result;
}

void LinkPreviewCache::put(const std::string& url, const LinkPreviewResult& result) {
    const std::lock_guard<std::mutex> lock(mutex_);
    entries_[url] = Entry{.result = result, .expiresAt = std::chrono::steady_clock::now() + ttl_};
}

}  // namespace chat_service
