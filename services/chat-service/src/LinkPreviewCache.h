#pragma once

#include <chrono>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

#include "OpenGraphParser.h"

namespace chat_service {

/// Результат превью для одного URL (issue #396) — available=false
/// означает "URL проверен, но превью получить не удалось" (невалидный
/// после SSRF-проверок, таймаут, не HTML, нет og-тегов), тоже
/// кэшируется, чтобы не дёргать одну и ту же нерабочую ссылку заново на
/// каждое открытие истории.
struct LinkPreviewResult {
    bool available = false;
    open_graph::Metadata metadata;
};

/**
 * @brief TTL-кэш результатов превью ссылок по URL, в памяти (тот же
 *        подход, что и у auth-service's OtpStore — мьютекс +
 *        unordered_map, отдельного персистентного хранилища не нужно:
 *        промах кэша просто означает ещё один поход во внешний
 *        интернет, а не потерю данных).
 */
class LinkPreviewCache {
public:
    explicit LinkPreviewCache(std::chrono::seconds ttl);

    /// @return закэшированный результат для @p url, если он ещё не
    /// истёк — nullopt при промахе (в том числе истёкшая запись).
    [[nodiscard]] std::optional<LinkPreviewResult> get(const std::string& url) const;

    /// Сохраняет/заменяет запись для @p url на @p result, действительную
    /// в течение ttl с этого момента.
    void put(const std::string& url, const LinkPreviewResult& result);

private:
    struct Entry {
        LinkPreviewResult result;
        std::chrono::steady_clock::time_point expiresAt;
    };

    std::chrono::seconds ttl_;
    mutable std::mutex mutex_;
    std::unordered_map<std::string, Entry> entries_;
};

}  // namespace chat_service
