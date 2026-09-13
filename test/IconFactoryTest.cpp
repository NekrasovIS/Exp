#include "ui/IconFactory.h"

#include <gtest/gtest.h>

#include <QColor>
#include <QString>

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

// Issue #418 — новые значки, заменяющие сырые эмодзи у кнопки записи
// голосового сообщения (mic/stop), индикатора закреплённого сообщения
// (pin), кнопки воспроизведения голосового сообщения (play/pause) и
// заглушки видео-вложения (video).

TEST(IconFactoryTest, MicIconIsNotNull) {
    const QIcon icon = ui_icons::micIcon(QColor("#ffffff"));

    EXPECT_FALSE(icon.isNull());
    EXPECT_FALSE(icon.availableSizes().isEmpty());
}

TEST(IconFactoryTest, StopIconIsNotNull) {
    const QIcon icon = ui_icons::stopIcon(QColor("#ffffff"));

    EXPECT_FALSE(icon.isNull());
    EXPECT_FALSE(icon.availableSizes().isEmpty());
}

TEST(IconFactoryTest, PinIconIsNotNull) {
    const QIcon icon = ui_icons::pinIcon(QColor("#ffffff"));

    EXPECT_FALSE(icon.isNull());
    EXPECT_FALSE(icon.availableSizes().isEmpty());
}

TEST(IconFactoryTest, PlayIconIsNotNull) {
    const QIcon icon = ui_icons::playIcon(QColor("#ffffff"));

    EXPECT_FALSE(icon.isNull());
    EXPECT_FALSE(icon.availableSizes().isEmpty());
}

TEST(IconFactoryTest, PauseIconIsNotNull) {
    const QIcon icon = ui_icons::pauseIcon(QColor("#ffffff"));

    EXPECT_FALSE(icon.isNull());
    EXPECT_FALSE(icon.availableSizes().isEmpty());
}

TEST(IconFactoryTest, VideoIconIsNotNull) {
    const QIcon icon = ui_icons::videoIcon(QColor("#ffffff"));

    EXPECT_FALSE(icon.isNull());
    EXPECT_FALSE(icon.availableSizes().isEmpty());
}

TEST(IconFactoryTest, IconHtmlProducesAnImgTagEmbeddingTheIcon) {
    const QString html = ui_icons::iconHtml(ui_icons::pinIcon(QColor("#ffffff")), 12);

    EXPECT_TRUE(html.startsWith(QStringLiteral("<img src=\"data:image/png;base64,")));
    EXPECT_TRUE(html.contains(QStringLiteral("width=\"12\"")));
}

}  // namespace
}  // namespace devicehub
