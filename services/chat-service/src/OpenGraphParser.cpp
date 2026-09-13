#include "OpenGraphParser.h"

#include <regex>

namespace chat_service::open_graph {

namespace {

/// Регистронезависимо ищет один `<meta ...>`-тег целиком (нежадно до
/// первого `>`) — атрибуты разбираются отдельно ниже, порядок
/// property/content внутри тега не имеет значения.
const std::regex kMetaTagPattern(R"(<meta\b[^>]*>)", std::regex::icase);

/// property="og:title" ИЛИ name="og:title" — некоторые страницы
/// ошибочно используют name вместо property для og:-тегов, оба
/// встречаются на практике.
std::regex propertyOrNameAttributePattern(const std::string& key) {
    return std::regex(R"((?:property|name)\s*=\s*["'])" + key + R"(["'])", std::regex::icase);
}

const std::regex kContentAttributePattern(R"(content\s*=\s*["']([^"']*)["'])", std::regex::icase);

const std::regex kTitleTagPattern(R"(<title[^>]*>([^<]*)</title>)", std::regex::icase);

std::string extractContent(const std::string& metaTag) {
    std::smatch match;
    if (std::regex_search(metaTag, match, kContentAttributePattern)) {
        return match[1].str();
    }
    return {};
}

std::string findMetaContent(const std::string& html, const std::string& propertyName) {
    const std::regex namePattern = propertyOrNameAttributePattern(propertyName);
    for (auto it = std::sregex_iterator(html.begin(), html.end(), kMetaTagPattern); it != std::sregex_iterator();
         ++it) {
        const std::string metaTag = it->str();
        if (std::regex_search(metaTag, namePattern)) {
            return extractContent(metaTag);
        }
    }
    return {};
}

std::string extractTitleTag(const std::string& html) {
    std::smatch match;
    if (std::regex_search(html, match, kTitleTagPattern)) {
        return match[1].str();
    }
    return {};
}

/// Упрощённое разрешение относительного @p imageUrl относительно
/// @p pageUrl — покрывает абсолютные ("https://..."), protocol-relative
/// ("//cdn...") и root-relative ("/img.png") формы og:image, которые на
/// практике покрывают подавляющее большинство страниц; полное
/// разрешение по RFC 3986 (относительные пути вида "../img.png") не
/// реализовано — такой URL возвращается как есть (лучше показать
/// нерабочую картинку, чем не показывать превью вовсе).
std::string resolveImageUrl(const std::string& imageUrl, const std::string& pageUrl) {
    if (imageUrl.empty()) {
        return imageUrl;
    }
    if (imageUrl.find("://") != std::string::npos) {
        return imageUrl;
    }
    const auto schemeEnd = pageUrl.find("://");
    if (schemeEnd == std::string::npos) {
        return imageUrl;
    }
    const std::string scheme = pageUrl.substr(0, schemeEnd);
    if (imageUrl.compare(0, 2, "//") == 0) {
        return scheme + ":" + imageUrl;
    }
    if (imageUrl.front() == '/') {
        const auto hostEnd = pageUrl.find('/', schemeEnd + 3);
        const std::string origin = hostEnd == std::string::npos ? pageUrl : pageUrl.substr(0, hostEnd);
        return origin + imageUrl;
    }
    return imageUrl;
}

}  // namespace

Metadata parse(const std::string& html, const std::string& pageUrl) {
    Metadata metadata;
    metadata.title = findMetaContent(html, "og:title");
    if (metadata.title.empty()) {
        metadata.title = extractTitleTag(html);
    }
    metadata.description = findMetaContent(html, "og:description");
    metadata.imageUrl = resolveImageUrl(findMetaContent(html, "og:image"), pageUrl);
    return metadata;
}

}  // namespace chat_service::open_graph
