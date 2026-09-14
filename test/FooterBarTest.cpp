#include "ui/FooterBar.h"

#include <gtest/gtest.h>

#include <QApplication>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QSignalSpy>

namespace devicehub {
namespace {

// Issue #415: аватар в футере теперь рисуется через
// IconFactory::communityAvatarIcon() (тот же путь, что и остальные
// аватары в приложении), а не выводится как текст QLabel — поэтому
// здесь, как и в IconFactoryTest, проверяется только что иконка
// действительно нарисована (непустой pixmap), без сэмплирования
// пикселей.

TEST(FooterBarTest, SetProfileTextUpdatesLabelAndRendersAvatarPixmap) {
    FooterBar bar;

    bar.setProfileText(QStringLiteral("alice"));

    EXPECT_EQ(bar.findChild<QLabel*>(QStringLiteral("footerProfileLabel"))->text(), QStringLiteral("alice"));
    EXPECT_FALSE(bar.findChild<QLabel*>(QStringLiteral("footerAvatar"))->pixmap().isNull());
}

TEST(FooterBarTest, SetProfileTextWithEmptyStringFallsBackToQuestionMarkAvatar) {
    FooterBar bar;
    bar.setProfileText(QStringLiteral("alice"));

    bar.setProfileText(QString());

    EXPECT_EQ(bar.findChild<QLabel*>(QStringLiteral("footerProfileLabel"))->text(), QString());
    EXPECT_FALSE(bar.findChild<QLabel*>(QStringLiteral("footerAvatar"))->pixmap().isNull());
}

TEST(FooterBarTest, ClickingAvatarEmitsAccountSettingsRequested) {
    FooterBar bar;
    QSignalSpy spy(&bar, &FooterBar::accountSettingsRequested);

    QMouseEvent press(QEvent::MouseButtonPress, QPointF(5, 5), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(bar.avatarLabel(), &press);

    EXPECT_EQ(spy.count(), 1);
}

TEST(FooterBarTest, AvatarHasPointingHandCursorToSignalItIsClickable) {
    FooterBar bar;

    EXPECT_EQ(bar.avatarLabel()->cursor().shape(), Qt::PointingHandCursor);
}

TEST(FooterBarTest, SettingsButtonExistsWithExpectedTooltip) {
    // Issue #182: значок вместо подписанной кнопки — текст описан
    // только в tooltip.
    FooterBar bar;

    ASSERT_NE(bar.settingsButton(), nullptr);
    EXPECT_EQ(bar.settingsButton()->toolTip(), QStringLiteral("Settings"));
}

}  // namespace
}  // namespace devicehub
