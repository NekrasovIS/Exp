#include "ui/ToastBanner.h"

#include <gtest/gtest.h>

#include <QEventLoop>
#include <QTimer>
#include <QWidget>

namespace devicehub {
namespace {

/// Spins the event loop for @p ms so queued timer events (like
/// ToastBanner's auto-hide) actually fire.
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
