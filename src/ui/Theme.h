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

/// Accent/foreground palette (issue #155) — QPainter-based drawing
/// (ChatBubble, ChatMessageRow, IconFactory) can't consume the QSS
/// string below, so it draws colors from these named constants instead
/// of restating hex literals; discordDarkStyleSheet() uses the same
/// values inline in its raw QSS string (kept in sync by hand — QSS
/// property values can't reference C++ constants).
constexpr const char* kAccentGradientStart = "#34d399";
constexpr const char* kAccentGradientEnd = "#059669";
constexpr const char* kAccentForeground = "#ffffff";
constexpr const char* kBubbleOtherBackground = "#2a2d31";

/// Qt stylesheet (QSS) implementing a dark theme with a green gradient
/// accent on primary actions: three layered dark backgrounds, light
/// text. Applied once via QApplication::setStyleSheet() in main.cpp.
QString discordDarkStyleSheet();

}  // namespace devicehub::ui_theme
