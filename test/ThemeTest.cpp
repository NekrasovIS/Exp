#include "ui/Theme.h"

#include <gtest/gtest.h>

#include <QString>

namespace devicehub {
namespace {

// Smoke test: this is a QSS stylesheet string applied wholesale via
// QApplication::setStyleSheet() — visual correctness is a rendering
// concern this project's tests don't verify (no pixel sampling); the
// only thing meaningfully assertable here is that it's non-empty.
TEST(ThemeTest, DiscordDarkStyleSheetIsNonEmpty) {
    const QString stylesheet = ui_theme::discordDarkStyleSheet();

    EXPECT_FALSE(stylesheet.isEmpty());
}

}  // namespace
}  // namespace devicehub
