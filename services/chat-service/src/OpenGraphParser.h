#pragma once

#include <string>

namespace chat_service::open_graph {

/// Метаданные, извлечённые из `<meta property="og:...">`-тегов HTML-
/// страницы (issue #396) — любое поле пусто, если соответствующий тег
/// отсутствует; title дополнительно берёт `<title>` как fallback, если
/// og:title не задан (многие страницы без og-разметки всё равно имеют
/// обычный `<title>`).
struct Metadata {
    std::string title;
    std::string description;
    std::string imageUrl;
};

/// Разбирает `<head>` секцию @p html в поисках og:title/og:description/
/// og:image (и `<title>` как fallback для title). Чисто текстовый
/// разбор (нет DOM-парсера как зависимости) — ищет `<meta ...>`-теги
/// регулярным по структуре, но не HTML-парсером сканированием, поэтому
/// не чувствителен к порядку атрибутов (`property` до или после
/// `content`). @p pageUrl используется только чтобы превратить
/// относительный og:image ("/img.png") в абсолютный URL — сама
/// страница не запрашивается повторно.
[[nodiscard]] Metadata parse(const std::string& html, const std::string& pageUrl);

}  // namespace chat_service::open_graph
