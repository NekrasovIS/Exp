#include "ui/StatusLabel.h"

#include <gtest/gtest.h>

#include <QLabel>

namespace devicehub {
namespace {

TEST(StatusLabelTest, SetsTextAndDefaultsToNeutralVariant) {
    QLabel label;

    ui_status::setStatusText(&label, QStringLiteral("Working..."));

    EXPECT_EQ(label.text(), QStringLiteral("Working..."));
    EXPECT_EQ(label.property("statusVariant").toString(), QStringLiteral("neutral"));
}

TEST(StatusLabelTest, SetsSuccessVariant) {
    QLabel label;

    ui_status::setStatusText(&label, QStringLiteral("Saved"), ui_status::Variant::kSuccess);

    EXPECT_EQ(label.text(), QStringLiteral("Saved"));
    EXPECT_EQ(label.property("statusVariant").toString(), QStringLiteral("success"));
}

TEST(StatusLabelTest, SetsErrorVariant) {
    QLabel label;

    ui_status::setStatusText(&label, QStringLiteral("Error: oops"), ui_status::Variant::kError);

    EXPECT_EQ(label.text(), QStringLiteral("Error: oops"));
    EXPECT_EQ(label.property("statusVariant").toString(), QStringLiteral("error"));
}

// Issue #417: эти статусные QLabel'ы живут дольше одного сообщения
// (ProfileDialog/ModeratorsDialog/LoginWindow/SettingsDialog переиспользуют
// их для каждого следующего события) — новый вызов должен полностью
// заменять вариант предыдущего, а не складывать/оставлять старый.
TEST(StatusLabelTest, ANewCallReplacesThePreviousVariant) {
    QLabel label;
    ui_status::setStatusText(&label, QStringLiteral("Error: oops"), ui_status::Variant::kError);

    ui_status::setStatusText(&label, QStringLiteral("Saved"), ui_status::Variant::kSuccess);

    EXPECT_EQ(label.property("statusVariant").toString(), QStringLiteral("success"));
}

}  // namespace
}  // namespace devicehub
