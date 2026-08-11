#include "ui/FooterBar.h"

#include <gtest/gtest.h>

#include <QLabel>

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

}  // namespace
}  // namespace devicehub
