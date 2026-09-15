#include "ui/IconFactory.h"

#include <gtest/gtest.h>

#include <QColor>
#include <QImage>
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

// Issue #384/#442 — реальное фото вместо буквы-заглушки.

TEST(IconFactoryTest, RealAvatarIconIsNotNull) {
    QImage image(64, 64, QImage::Format_ARGB32);
    image.fill(Qt::red);

    const QIcon icon = ui_icons::realAvatarIcon(image);

    EXPECT_FALSE(icon.isNull());
    EXPECT_FALSE(icon.availableSizes().isEmpty());
}

TEST(IconFactoryTest, RealAvatarIconHandlesANonSquareSourceImageWithoutCrashing) {
    // issue #442: центрируется и обрезается по короткой стороне —
    // не должно ни падать, ни искажаться в эллипс на неквадратном фото.
    QImage wideImage(120, 40, QImage::Format_ARGB32);
    wideImage.fill(Qt::blue);
    QImage tallImage(40, 120, QImage::Format_ARGB32);
    tallImage.fill(Qt::green);

    EXPECT_FALSE(ui_icons::realAvatarIcon(wideImage).isNull());
    EXPECT_FALSE(ui_icons::realAvatarIcon(tallImage).isNull());
}

TEST(IconFactoryTest, RealMemberAvatarIconIsNotNullWithAndWithoutThePresenceDot) {
    QImage image(64, 64, QImage::Format_ARGB32);
    image.fill(Qt::red);

    const QIcon onlineIcon = ui_icons::realMemberAvatarIcon(image, /*online=*/true);
    const QIcon offlineIcon = ui_icons::realMemberAvatarIcon(image, /*online=*/false);

    EXPECT_FALSE(onlineIcon.isNull());
    EXPECT_FALSE(offlineIcon.isNull());
    // Смысловая разница между двумя состояниями — presence-точка;
    // сравнение содержимого пикселей этот проект в тестах избегает (см.
    // doc-комментарий выше), но хотя бы сама точка должна физически
    // изменить итоговый рисунок, а не быть отрисована и тут же стёрта.
    EXPECT_NE(onlineIcon.pixmap(40, 40).toImage(), offlineIcon.pixmap(40, 40).toImage());
}

TEST(IconFactoryTest, IconHtmlProducesAnImgTagEmbeddingTheIcon) {
    const QString html = ui_icons::iconHtml(ui_icons::pinIcon(QColor("#ffffff")), 12);

    EXPECT_TRUE(html.startsWith(QStringLiteral("<img src=\"data:image/png;base64,")));
    EXPECT_TRUE(html.contains(QStringLiteral("width=\"12\"")));
}

}  // namespace
}  // namespace devicehub
