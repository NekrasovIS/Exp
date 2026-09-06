#include "devices/ScreenCaptureDevice.h"

#include <gtest/gtest.h>

#include <QEventLoop>
#include <QGuiApplication>
#include <QScreen>
#include <QTimer>

namespace devicehub {
namespace {

// Живой захват экрана (issue #154) зависит от реальной GPU/дисплейной
// подсистемы (Windows.Graphics.Capture поверх DXGI) — на CI-раннере без
// неё или без физического дисплея захват может быть недоступен так же,
// как это уже допускается для камеры/микрофона в других тестах (см.
// GTEST_SKIP в CallManagerIntegrationTest), а не считается провалом.
TEST(ScreenCaptureDeviceTest, CapturingThePrimaryScreenProducesAtLeastOneFrame) {
    QScreen* primaryScreen = QGuiApplication::primaryScreen();
    if (primaryScreen == nullptr) {
        GTEST_SKIP() << "No primary screen available in this environment.";
    }

    ScreenCaptureDevice device;
    QStringList errors;
    QObject::connect(&device, &ScreenCaptureDevice::errorOccurred,
                      [&](const QString& message) { errors << message; });

    QVideoFrame receivedFrame;
    QObject::connect(&device, &ScreenCaptureDevice::frameAvailable,
                      [&](const QVideoFrame& frame) { receivedFrame = frame; });

    device.setScreen(primaryScreen);
    device.start();

    {
        QEventLoop loop;
        QTimer::singleShot(3000, &loop, &QEventLoop::quit);
        QObject::connect(&device, &ScreenCaptureDevice::frameAvailable, &loop, &QEventLoop::quit);
        loop.exec();
    }

    device.stop();

    if (!errors.isEmpty()) {
        GTEST_SKIP() << "Screen capture unavailable in this environment: " << errors.join(QStringLiteral("; ")).toStdString();
    }
    ASSERT_TRUE(receivedFrame.isValid()) << "No frame received within timeout.";
    EXPECT_GT(receivedFrame.width(), 0);
    EXPECT_GT(receivedFrame.height(), 0);
}

}  // namespace
}  // namespace devicehub
