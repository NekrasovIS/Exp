#include "LinkPreviewService.h"

namespace chat_service {

LinkPreviewService::LinkPreviewService(std::chrono::seconds cacheTtl, std::chrono::seconds requestTimeout,
                                        std::size_t maxResponseBytes, int maxRedirects)
    : fetcher_(requestTimeout, maxResponseBytes, maxRedirects), cache_(cacheTtl) {}

LinkPreviewResult LinkPreviewService::getPreview(const std::string& url) {
    if (const std::optional<LinkPreviewResult> cached = cache_.get(url); cached.has_value()) {
        return *cached;
    }

    LinkPreviewResult result;
    if (const std::optional<std::string> html = fetcher_.fetch(url); html.has_value()) {
        result.available = true;
        result.metadata = open_graph::parse(*html, url);
    }

    cache_.put(url, result);
    return result;
}

}  // namespace chat_service
