#pragma once

#include <chrono>
#include <cstddef>
#include <string>

#include "LinkPreviewCache.h"
#include "LinkPreviewFetcher.h"

namespace chat_service {

/**
 * @brief Оркестрирует превью ссылок (issue #396) — единственная точка
 *        входа для HttpServer: кэш, скачивание (LinkPreviewFetcher,
 *        SSRF-безопасно), разбор og-тегов (open_graph::parse()), запись
 *        результата обратно в кэш. Тот же принцип разделения, что и у
 *        ChatService — HttpServer не трогает LinkPreviewFetcher/
 *        LinkPreviewCache напрямую.
 *
 * Владеет fetcher_/cache_ по значению, а не принимает их
 * сконструированными снаружи — оба содержат непереносимые
 * (LinkPreviewCache — std::mutex) или намеренно приватные для
 * компоновки члены, так что этот класс — их единственная точка сборки.
 */
class LinkPreviewService {
public:
    /// Параметры — см. одноимённые у LinkPreviewFetcher/LinkPreviewCache.
    explicit LinkPreviewService(std::chrono::seconds cacheTtl = std::chrono::hours{1},
                                 std::chrono::seconds requestTimeout = std::chrono::seconds{5},
                                 std::size_t maxResponseBytes = 256 * 1024, int maxRedirects = 3);

    /// @return результат превью для @p url — из кэша, если он там ещё
    /// свежий, иначе скачивает (LinkPreviewFetcher — сам отклоняет
    /// недопустимые схемы/приватные адреса) и разбирает заново, сохраняя
    /// результат в кэш перед возвратом (включая available=false —
    /// нерабочая ссылка тоже не должна дёргаться повторно).
    [[nodiscard]] LinkPreviewResult getPreview(const std::string& url);

private:
    LinkPreviewFetcher fetcher_;
    LinkPreviewCache cache_;
};

}  // namespace chat_service
