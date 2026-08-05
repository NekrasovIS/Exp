#include "devices/CallAudioDeviceModule.h"

#include <api/audio/audio_device_defines.h>
#include <api/make_ref_counted.h>

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>
#include <vector>

namespace devicehub {
namespace {

/// Hand-written webrtc::AudioTransport double — lets CallAudioDeviceModule's
/// business logic (callback registration, playout-loop cadence, capture
/// forwarding) be exercised without a real PeerConnection or audio hardware.
class FakeAudioTransport : public webrtc::AudioTransport {
public:
    int32_t RecordedDataIsAvailable(const void* audioSamples, size_t nSamples, size_t /*nBytesPerSample*/,
                                     size_t nChannels, uint32_t samplesPerSec, uint32_t /*totalDelayMS*/,
                                     int32_t /*clockDrift*/, uint32_t /*currentMicLevel*/, bool /*keyPressed*/,
                                     uint32_t& /*newMicLevel*/) override {
        const std::lock_guard<std::mutex> lock(mutex_);
        lastRecordedSamples_.assign(static_cast<const int16_t*>(audioSamples),
                                     static_cast<const int16_t*>(audioSamples) + (nSamples * nChannels));
        lastRecordedSampleRate_ = static_cast<int>(samplesPerSec);
        lastRecordedChannels_ = nChannels;
        ++recordedCallCount_;
        cv_.notify_all();
        return 0;
    }

    int32_t NeedMorePlayData(size_t nSamples, size_t /*nBytesPerSample*/, size_t nChannels, uint32_t /*samplesPerSec*/,
                              void* audioSamples, size_t& nSamplesOut, int64_t* elapsed_time_ms,
                              int64_t* ntp_time_ms) override {
        auto* out = static_cast<int16_t*>(audioSamples);
        for (size_t i = 0; i < nSamples * nChannels; ++i) {
            out[i] = static_cast<int16_t>(42);
        }
        nSamplesOut = nSamples;
        *elapsed_time_ms = 0;
        *ntp_time_ms = 0;
        {
            const std::lock_guard<std::mutex> lock(mutex_);
            ++playoutCallCount_;
        }
        cv_.notify_all();
        return 0;
    }

    void PullRenderData(int /*bits_per_sample*/, int /*sample_rate*/, size_t /*number_of_channels*/,
                         size_t /*number_of_frames*/, void* /*audio_data*/, int64_t* /*elapsed_time_ms*/,
                         int64_t* /*ntp_time_ms*/) override {}

    [[nodiscard]] int playoutCallCount() const {
        const std::lock_guard<std::mutex> lock(mutex_);
        return playoutCallCount_;
    }

    [[nodiscard]] int recordedCallCount() const {
        const std::lock_guard<std::mutex> lock(mutex_);
        return recordedCallCount_;
    }

    [[nodiscard]] std::vector<int16_t> lastRecordedSamples() const {
        const std::lock_guard<std::mutex> lock(mutex_);
        return lastRecordedSamples_;
    }

    [[nodiscard]] int lastRecordedSampleRate() const {
        const std::lock_guard<std::mutex> lock(mutex_);
        return lastRecordedSampleRate_;
    }

    [[nodiscard]] size_t lastRecordedChannels() const {
        const std::lock_guard<std::mutex> lock(mutex_);
        return lastRecordedChannels_;
    }

    bool waitForPlayoutCalls(int minCount, int timeoutMs = 2000) {
        std::unique_lock<std::mutex> lock(mutex_);
        return cv_.wait_for(lock, std::chrono::milliseconds(timeoutMs),
                             [this, minCount] { return playoutCallCount_ >= minCount; });
    }

private:
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    int playoutCallCount_ = 0;
    int recordedCallCount_ = 0;
    std::vector<int16_t> lastRecordedSamples_;
    int lastRecordedSampleRate_ = 0;
    size_t lastRecordedChannels_ = 0;
};

}  // namespace

TEST(CallAudioDeviceModuleTest, PlayoutLoopPullsAndForwardsToSink) {
    std::atomic<int> sinkCallCount{0};
    std::atomic<std::thread::id> sinkThreadId{};
    const std::thread::id testThreadId = std::this_thread::get_id();

    auto adm = webrtc::make_ref_counted<CallAudioDeviceModule>(
        [&sinkCallCount, &sinkThreadId](const int16_t* /*samples*/, size_t frameCount, int sampleRateHz,
                                         size_t channels) {
            EXPECT_GT(frameCount, 0U);
            EXPECT_EQ(sampleRateHz, 48000);
            EXPECT_EQ(channels, 1U);
            sinkThreadId.store(std::this_thread::get_id());
            ++sinkCallCount;
        });

    FakeAudioTransport transport;
    ASSERT_EQ(adm->RegisterAudioCallback(&transport), 0);

    bool playoutAvailable = false;
    ASSERT_EQ(adm->PlayoutIsAvailable(&playoutAvailable), 0);
    EXPECT_TRUE(playoutAvailable);

    ASSERT_EQ(adm->InitPlayout(), 0);
    EXPECT_TRUE(adm->PlayoutIsInitialized());
    ASSERT_EQ(adm->StartPlayout(), 0);
    EXPECT_TRUE(adm->Playing());

    EXPECT_TRUE(transport.waitForPlayoutCalls(3));

    ASSERT_EQ(adm->StopPlayout(), 0);
    EXPECT_FALSE(adm->Playing());

    EXPECT_GE(sinkCallCount.load(), 3);
    EXPECT_NE(sinkThreadId.load(), testThreadId);
}

TEST(CallAudioDeviceModuleTest, PushCapturedAudioForwardsOnlyWhileRecording) {
    auto adm = webrtc::make_ref_counted<CallAudioDeviceModule>(CallAudioDeviceModule::PlayoutSink{});

    FakeAudioTransport transport;
    ASSERT_EQ(adm->RegisterAudioCallback(&transport), 0);

    const std::vector<int16_t> samples{1, 2, 3, 4};

    // Not recording yet — pushed audio is dropped.
    adm->pushCapturedAudio(samples.data(), samples.size(), 16000, 1);
    EXPECT_EQ(transport.recordedCallCount(), 0);

    bool recordingAvailable = false;
    ASSERT_EQ(adm->RecordingIsAvailable(&recordingAvailable), 0);
    EXPECT_TRUE(recordingAvailable);

    ASSERT_EQ(adm->InitRecording(), 0);
    EXPECT_TRUE(adm->RecordingIsInitialized());
    ASSERT_EQ(adm->StartRecording(), 0);
    EXPECT_TRUE(adm->Recording());

    adm->pushCapturedAudio(samples.data(), samples.size(), 16000, 1);
    ASSERT_EQ(transport.recordedCallCount(), 1);
    EXPECT_EQ(transport.lastRecordedSamples(), samples);
    EXPECT_EQ(transport.lastRecordedSampleRate(), 16000);
    EXPECT_EQ(transport.lastRecordedChannels(), 1U);

    ASSERT_EQ(adm->StopRecording(), 0);
    EXPECT_FALSE(adm->Recording());

    adm->pushCapturedAudio(samples.data(), samples.size(), 16000, 1);
    EXPECT_EQ(transport.recordedCallCount(), 1);
}

}  // namespace devicehub
