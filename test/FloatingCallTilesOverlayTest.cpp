#include "ui/FloatingCallTilesOverlay.h"

#include <gtest/gtest.h>

#include <QPushButton>
#include <QSignalSpy>

namespace devicehub {
namespace {

TEST(FloatingCallTilesOverlayTest, CanvasAndRestoreButtonExist) {
    FloatingCallTilesOverlay overlay;

    EXPECT_NE(overlay.canvas(), nullptr);
    ASSERT_NE(overlay.restoreButton(), nullptr);
    EXPECT_EQ(overlay.restoreButton()->text(), QStringLiteral("Expand"));
}

TEST(FloatingCallTilesOverlayTest, ClickingRestoreButtonEmitsRestoreRequested) {
    FloatingCallTilesOverlay overlay;
    QSignalSpy spy(&overlay, &FloatingCallTilesOverlay::restoreRequested);

    emit overlay.restoreButton()->clicked();

    EXPECT_EQ(spy.count(), 1);
}

}  // namespace
}  // namespace devicehub
