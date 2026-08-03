#include "devices/DeviceEnumerator.h"

#include <gtest/gtest.h>

namespace devicehub {
namespace {

TEST(DeviceEnumeratorTest, AudioOutputsReturnsAListWithoutCrashing) {
    DeviceEnumerator enumerator;
    const QList<QAudioDevice> outputs = enumerator.audioOutputs();
    EXPECT_GE(outputs.size(), 0);
}

TEST(DeviceEnumeratorTest, AudioInputsReturnsAListWithoutCrashing) {
    DeviceEnumerator enumerator;
    const QList<QAudioDevice> inputs = enumerator.audioInputs();
    EXPECT_GE(inputs.size(), 0);
}

TEST(DeviceEnumeratorTest, CamerasReturnsAListWithoutCrashing) {
    DeviceEnumerator enumerator;
    const QList<QCameraDevice> cameras = enumerator.cameras();
    EXPECT_GE(cameras.size(), 0);
}

TEST(DeviceEnumeratorTest, ScreensReturnsAListWithoutCrashing) {
    DeviceEnumerator enumerator;
    const QList<QScreen*> screens = enumerator.screens();
    EXPECT_GE(screens.size(), 0);
}

}  // namespace
}  // namespace devicehub
