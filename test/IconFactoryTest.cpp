#include "ui/IconFactory.h"

#include <gtest/gtest.h>

#include <QColor>

namespace devicehub {
namespace {

// Smoke tests only — these icons are hand-painted with QPainter rather
// than loaded from a resource, so the only thing meaningfully
// assertable without pixel sampling (a rendering concern this
// project's tests avoid) is that a real, non-empty icon comes back.

TEST(IconFactoryTest, PlusIconIsNotNull) {
    const QIcon icon = ui_icons::plusIcon(QColor("#ffffff"));

    EXPECT_FALSE(icon.isNull());
    EXPECT_FALSE(icon.availableSizes().isEmpty());
}

TEST(IconFactoryTest, RefreshIconIsNotNull) {
    const QIcon icon = ui_icons::refreshIcon();

    EXPECT_FALSE(icon.isNull());
}

TEST(IconFactoryTest, CommunityAvatarIconIsNotNull) {
    const QIcon icon = ui_icons::communityAvatarIcon(QStringLiteral("A"));

    EXPECT_FALSE(icon.isNull());
    EXPECT_FALSE(icon.availableSizes().isEmpty());
}

}  // namespace
}  // namespace devicehub
