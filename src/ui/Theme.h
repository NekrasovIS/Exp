#pragma once

class QString;

namespace devicehub::ui_theme {

/// Qt stylesheet (QSS) implementing a Discord-like dark theme: three
/// layered dark backgrounds, light text, a blurple accent on primary
/// actions. Applied once via QApplication::setStyleSheet() in main.cpp.
QString discordDarkStyleSheet();

}  // namespace devicehub::ui_theme
