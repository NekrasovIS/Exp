#pragma once

class QString;

namespace devicehub::ui_theme {

/// Layout spacing scale — every setContentsMargins()/setSpacing() call
/// and every QSS padding in discordDarkStyleSheet() draws from these
/// three steps rather than ad hoc pixel values, so spacing stays
/// visually consistent across panels.
constexpr int kSpacingSm = 8;
constexpr int kSpacingMd = 12;
constexpr int kSpacingLg = 16;

/// Qt stylesheet (QSS) implementing a dark theme with a green gradient
/// accent on primary actions: three layered dark backgrounds, light
/// text. Applied once via QApplication::setStyleSheet() in main.cpp.
QString discordDarkStyleSheet();

}  // namespace devicehub::ui_theme
