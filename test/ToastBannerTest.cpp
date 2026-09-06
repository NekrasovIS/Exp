#include "ui/ToastBanner.h"

#include <gtest/gtest.h>

#include <QEventLoop>
#include <QLabel>
#include <QTimer>
#include <QWidget>

namespace devicehub {
namespace {

/// Крутит цикл событий в течение @p ms, чтобы поставленные в очередь события
/// таймеров (например, авто-скрытие ToastBanner) успели сработать.
void waitMs(int ms) {
    QEventLoop loop;
    QTimer::singleShot(ms, &loop, &QEventLoop::quit);
    loop.exec();
}

TEST(ToastBannerTest, ShowMessageMakesItVisibleThenAutoHides) {
    QWidget parent;
    ToastBanner banner(&parent);

    EXPECT_TRUE(banner.isHidden());

    banner.showMessage(QStringLiteral("Done"), ToastBanner::Variant::kSuccess, /*timeoutMs=*/30);
    EXPECT_FALSE(banner.isHidden());

    waitMs(150);
    EXPECT_TRUE(banner.isHidden());
}

TEST(ToastBannerTest, ErrorVariantSetsTheVariantProperty) {
    QWidget parent;
    ToastBanner banner(&parent);

    banner.showMessage(QStringLiteral("Something broke"), ToastBanner::Variant::kError, /*timeoutMs=*/5000);

    EXPECT_EQ(banner.property("variant").toString(), QStringLiteral("error"));
    EXPECT_EQ(banner.findChild<QLabel*>()->text(), QStringLiteral("Something broke"));
}

TEST(ToastBannerTest, CallingAgainWhileVisibleRestartsTimeoutWithNewTextAndVariant) {
    QWidget parent;
    ToastBanner banner(&parent);

    banner.showMessage(QStringLiteral("First"), ToastBanner::Variant::kInfo, /*timeoutMs=*/5000);
    ASSERT_FALSE(banner.isHidden());

    // Второй вызов приходит задолго до того, как сработает 5-секундный тайм-аут
    // первого — определять поведение должен именно его собственный, гораздо
    // более короткий тайм-аут.
    banner.showMessage(QStringLiteral("Second"), ToastBanner::Variant::kError, /*timeoutMs=*/30);

    EXPECT_EQ(banner.findChild<QLabel*>()->text(), QStringLiteral("Second"));
    EXPECT_EQ(banner.property("variant").toString(), QStringLiteral("error"));

    waitMs(150);
    EXPECT_TRUE(banner.isHidden());
}

TEST(ToastBannerTest, RepositionsWhenParentResizes) {
    QWidget parent;
    parent.resize(400, 300);
    parent.show();
    ToastBanner banner(&parent);

    banner.showMessage(QStringLiteral("Resized"), ToastBanner::Variant::kInfo, /*timeoutMs=*/5000);
    const int widthBeforeResize = banner.width();
    ASSERT_LT(widthBeforeResize, parent.width());

    parent.resize(800, 300);
    waitMs(20);
    EXPECT_GT(banner.width(), widthBeforeResize);
    EXPECT_LT(banner.width(), parent.width());
}

}  // namespace
}  // namespace devicehub
