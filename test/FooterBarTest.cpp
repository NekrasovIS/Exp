#include "ui/FooterBar.h"

#include <gtest/gtest.h>

#include <QApplication>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QSignalSpy>

namespace devicehub {
namespace {

TEST(FooterBarTest, SetProfileTextUpdatesLabelAndAvatarInitial) {
    FooterBar bar;

    bar.setProfileText(QStringLiteral("alice"));

    EXPECT_EQ(bar.findChild<QLabel*>(QStringLiteral("footerProfileLabel"))->text(), QStringLiteral("alice"));
    EXPECT_EQ(bar.findChild<QLabel*>(QStringLiteral("footerAvatar"))->text(), QStringLiteral("A"));
}

TEST(FooterBarTest, SetProfileTextWithEmptyStringFallsBackToQuestionMark) {
    FooterBar bar;
    bar.setProfileText(QStringLiteral("alice"));

    bar.setProfileText(QString());

    EXPECT_EQ(bar.findChild<QLabel*>(QStringLiteral("footerProfileLabel"))->text(), QString());
    EXPECT_EQ(bar.findChild<QLabel*>(QStringLiteral("footerAvatar"))->text(), QStringLiteral("?"));
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

TEST(FooterBarTest, SettingsButtonExistsWithExpectedLabel) {
    FooterBar bar;

    ASSERT_NE(bar.settingsButton(), nullptr);
    EXPECT_EQ(bar.settingsButton()->text(), QStringLiteral("⚙ Settings"));
}

}  // namespace
}  // namespace devicehub
