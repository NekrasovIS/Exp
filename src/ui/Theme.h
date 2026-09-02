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

/// Таблица стилей Qt (QSS), реализующая тёмную тему с зелёным
/// градиентным акцентом на основных действиях: три слоя тёмных фонов,
/// светлый текст. Применяется один раз через
/// QApplication::setStyleSheet() в main.cpp.
QString discordDarkStyleSheet();

}  // namespace devicehub::ui_theme
