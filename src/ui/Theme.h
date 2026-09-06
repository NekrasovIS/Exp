#pragma once

class QString;

namespace devicehub::ui_theme {

/// Шкала отступов layout'а — каждый вызов setContentsMargins()/
/// setSpacing() и каждый padding в QSS в discordDarkStyleSheet() берётся
/// из этих трёх шагов, а не из произвольных пиксельных значений, так
/// что отступы остаются визуально согласованными между панелями.
constexpr int kSpacingSm = 8;
constexpr int kSpacingMd = 12;
constexpr int kSpacingLg = 16;

/// Палитра акцента/переднего плана (issue #155) — рисование через
/// QPainter (ChatBubble, ChatMessageRow, IconFactory) не может
/// использовать QSS-строку ниже, поэтому берёт цвета из этих именованных
/// констант вместо повторения hex-литералов; discordDarkStyleSheet()
/// использует те же значения инлайн в своей сырой QSS-строке (держатся
/// в синхроне вручную — значения свойств QSS не могут ссылаться на
/// C++-константы).
constexpr const char* kAccentGradientStart = "#34d399";
constexpr const char* kAccentGradientEnd = "#059669";
constexpr const char* kAccentForeground = "#ffffff";
constexpr const char* kBubbleOtherBackground = "#2a2d31";

/// Таблица стилей Qt (QSS), реализующая тёмную тему с зелёным
/// градиентным акцентом на основных действиях: три слоя тёмных фонов,
/// светлый текст. Применяется один раз через
/// QApplication::setStyleSheet() в main.cpp.
QString discordDarkStyleSheet();

}  // namespace devicehub::ui_theme
