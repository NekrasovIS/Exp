#include "devices/VoiceMessageRecorder.h"

#include <gtest/gtest.h>

// Как и AudioInputDevice, которую этот класс оборачивает, не запускает
// реальный захват с микрофона в тестах (голый тестовый бинарник не
// имеет разрешений ОС на это) — проверяет только состояние,
// достижимое без единого вызова start() к настоящему QAudioSource.

namespace devicehub {
namespace {

TEST(VoiceMessageRecorderTest, IsNotRecordingInitially) {
    VoiceMessageRecorder recorder;

    EXPECT_FALSE(recorder.isRecording());
}

TEST(VoiceMessageRecorderTest, StopWithoutStartReturnsEmptyAndDoesNotCrash) {
    VoiceMessageRecorder recorder;

    const QByteArray result = recorder.stop();

    EXPECT_TRUE(result.isEmpty());
    EXPECT_FALSE(recorder.isRecording());
}

}  // namespace
}  // namespace devicehub
