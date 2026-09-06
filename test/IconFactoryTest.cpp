#include "ui/IconFactory.h"

#include <gtest/gtest.h>

#include <QColor>

namespace devicehub {
namespace {

// Только smoke-тесты — эти иконки рисуются вручную через QPainter, а не
// загружаются из ресурса, поэтому единственное, что можно осмысленно
// проверить без сэмплирования пикселей (вопрос рендеринга, которого этот
// проект в тестах избегает), — это что возвращается реальная, непустая
// иконка.

TEST(IconFactoryTest, PlusIconIsNotNull) {
    const QIcon icon = ui_icons::plusIcon(QColor("#ffffff"));

    EXPECT_FALSE(icon.isNull());
    EXPECT_FALSE(icon.availableSizes().isEmpty());
}

TEST(IconFactoryTest, CommunityAvatarIconIsNotNull) {
    const QIcon icon = ui_icons::communityAvatarIcon(QStringLiteral("A"));

    EXPECT_FALSE(icon.isNull());
    EXPECT_FALSE(icon.availableSizes().isEmpty());
}

TEST(IconFactoryTest, MembersIconIsNotNull) {
    const QIcon icon = ui_icons::membersIcon(QColor("#ffffff"));

    EXPECT_FALSE(icon.isNull());
    EXPECT_FALSE(icon.availableSizes().isEmpty());
}

}  // namespace
}  // namespace devicehub
