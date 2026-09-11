#pragma once

class QString;

namespace devicehub::message_formatting {

/// Оборачивает каждое вхождение `@login` (issue #307) в markdown
/// `**@login**`, чтобы уже существующий Qt::MarkdownText-рендер
/// ChatMessageRow (issue #94) сам отрисовал его полужирным — без
/// собственного парсера rich text. `login` — последовательность букв/
/// цифр/`_`/`-`, тот же алфавит, что допускают логины user-service.
/// Не проверяет, что упомянутый login реально существует или состоит в
/// сообществе — сообщения не имеют доступа к списку участников на этом
/// уровне, а лишний false positive (упомянут кто-то посторонний) не
/// несёт риска, в отличие от пропущенного упоминания.
[[nodiscard]] QString highlightMentions(const QString& body);

}  // namespace devicehub::message_formatting
