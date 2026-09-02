#include "ui/Theme.h"

#include <gtest/gtest.h>

#include <QString>

namespace devicehub {
namespace {

// Smoke-тест: это строка QSS-стилей, применяемая целиком через
// QApplication::setStyleSheet() — визуальная корректность является
// вопросом рендеринга, который тесты этого проекта не проверяют (нет
// сэмплирования пикселей); единственное, что здесь можно осмысленно
// проверить, — это что строка непустая.
TEST(ThemeTest, DiscordDarkStyleSheetIsNonEmpty) {
    const QString stylesheet = ui_theme::discordDarkStyleSheet();

    EXPECT_FALSE(stylesheet.isEmpty());
}

}  // namespace
}  // namespace devicehub
