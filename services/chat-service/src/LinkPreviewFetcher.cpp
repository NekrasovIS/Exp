#include "LinkPreviewFetcher.h"

#include <httplib.h>

#include <algorithm>

#include "PrivateNetworkCheck.h"

namespace chat_service {

namespace {

/// Location из ответа 3xx может быть относительным — разрешает его
/// относительно @p currentUrl. Тот же уровень упрощения, что и у
/// OpenGraphParser::resolveImageUrl() (абсолютные/root-relative/
/// same-directory формы, без полного RFC 3986).
std::string resolveRedirectUrl(const std::string& currentUrl, const std::string& location) {
    if (location.find("://") != std::string::npos) {
        return location;
    }
    httplib::detail::UrlComponents currentComponents;
    if (!httplib::detail::parse_url(currentUrl, currentComponents) || currentComponents.host.empty()) {
        return location;
    }
    const std::string origin = currentComponents.scheme + "://" + currentComponents.host +
                                (currentComponents.port.empty() ? "" : ":" + currentComponents.port);
    if (!location.empty() && location.front() == '/') {
        return origin + location;
    }
    const auto lastSlash = currentComponents.path.find_last_of('/');
    const std::string directory =
        lastSlash == std::string::npos ? "/" : currentComponents.path.substr(0, lastSlash + 1);
    return origin + directory + location;
}

}  // namespace

bool LinkPreviewFetcher::defaultIsAddressAllowed(const std::string& resolvedIp) {
    return !private_network_check::isPrivateOrReserved(resolvedIp);
}

LinkPreviewFetcher::LinkPreviewFetcher(std::chrono::seconds requestTimeout, std::size_t maxResponseBytes,
                                        int maxRedirects, std::function<bool(const std::string&)> isAddressAllowed)
    : requestTimeout_(requestTimeout),
      maxResponseBytes_(maxResponseBytes),
      maxRedirects_(maxRedirects),
      isAddressAllowed_(std::move(isAddressAllowed)) {}

std::optional<std::string> LinkPreviewFetcher::fetch(const std::string& url) const {
    std::string currentUrl = url;

    for (int hop = 0; hop <= maxRedirects_; ++hop) {
        httplib::detail::UrlComponents components;
        if (!httplib::detail::parse_url(currentUrl, components) || components.host.empty()) {
            return std::nullopt;
        }
        if (components.scheme != "http" && components.scheme != "https") {
            return std::nullopt;
        }

        // Резолвится и проверяется здесь, заново на каждом хопе — см.
        // doc-комментарий класса про TOCTOU/DNS rebinding: httplib ниже
        // подключается ровно к этому проверенному адресу
        // (set_hostname_addr_map()), не резолвя хост ещё раз сам.
        const std::optional<std::string> resolvedIp = private_network_check::resolveFirstAddress(components.host);
        if (!resolvedIp.has_value() || !isAddressAllowed_(*resolvedIp)) {
            return std::nullopt;
        }

        const std::string schemeHostPort =
            components.scheme + "://" + components.host + (components.port.empty() ? "" : ":" + components.port);
        httplib::Client client(schemeHostPort);
        client.set_hostname_addr_map({{components.host, *resolvedIp}});
        client.set_connection_timeout(requestTimeout_);
        client.set_read_timeout(requestTimeout_);
        client.set_follow_location(false);
        client.enable_server_certificate_verification(true);

        std::string body;
        const std::string pathAndQuery = (components.path.empty() ? "/" : components.path) + components.query;
        // Content receiver всегда возвращает true (не обрывает
        // соединение) — httplib трактует false как ошибку и делает весь
        // httplib::Result недействительным (Result::res_ == nullptr),
        // так что после этого не восстановить ни статус ответа, ни уже
        // накопленное тело. Вместо обрыва — просто перестаём
        // дописывать в body после лимита; og-теги всё равно уже
        // получены (они в <head>) к этому моменту на подавляющем
        // большинстве страниц.
        const httplib::Result result =
            client.Get(pathAndQuery, httplib::Headers{}, [&](const char* data, std::size_t length) {
                if (body.size() < maxResponseBytes_) {
                    const std::size_t remaining = maxResponseBytes_ - body.size();
                    body.append(data, std::min(length, remaining));
                }
                return true;
            });

        if (!result) {
            return std::nullopt;
        }
        if (result->status >= 300 && result->status < 400) {
            const std::string location = result->get_header_value("Location");
            if (location.empty()) {
                return std::nullopt;
            }
            currentUrl = resolveRedirectUrl(currentUrl, location);
            continue;
        }
        if (result->status < 200 || result->status >= 300) {
            return std::nullopt;
        }
        return body;
    }

    return std::nullopt;  // Слишком много редиректов.
}

}  // namespace chat_service
