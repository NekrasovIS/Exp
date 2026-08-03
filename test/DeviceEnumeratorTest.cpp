#include "devices/DeviceEnumerator.h"

#include <gtest/gtest.h>

#include <QCoreApplication>

namespace devicehub {
namespace {

// QMediaDevices needs a QCoreApplication event loop context to initialize
// its backend; GTest's main() alone isn't enough, so provide one here.
class QtEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        static int argc = 1;
        static char argv0[] = "devicehub_tests";
        static char* argv[] = {argv0};
        if (QCoreApplication::instance() == nullptr) {
            app_ = std::make_unique<QCoreApplication>(argc, argv);
        }
    }

private:
    std::unique_ptr<QCoreApplication> app_;
};

const ::testing::Environment* const kQtEnv = ::testing::AddGlobalTestEnvironment(new QtEnvironment());

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

}  // namespace
}  // namespace devicehub
