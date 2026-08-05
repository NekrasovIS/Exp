#include "CallAudioDeviceModule.h"

#include <chrono>
#include <utility>
#include <vector>

namespace devicehub {

namespace {
constexpr int kPlayoutIntervalMs = 10;
}  // namespace

CallAudioDeviceModule::CallAudioDeviceModule(PlayoutSink playoutSink) : playoutSink_(std::move(playoutSink)) {}

CallAudioDeviceModule::~CallAudioDeviceModule() {
    StopPlayout();
}

void CallAudioDeviceModule::setPlayoutFormat(int sampleRateHz, size_t channels) {
    playoutSampleRateHz_ = sampleRateHz;
    playoutChannels_ = channels;
}

webrtc::AudioTransport* CallAudioDeviceModule::transport() const {
    const std::lock_guard<std::mutex> lock(transportMutex_);
    return transport_;
}

void CallAudioDeviceModule::pushCapturedAudio(const int16_t* samples, size_t frameCount, int sampleRateHz,
                                               size_t channels) {
    if (!recording_.load()) {
        return;
    }
    webrtc::AudioTransport* callback = transport();
    if (callback == nullptr) {
        return;
    }
    uint32_t newMicLevel = 0;
    // nBytesPerSample here means bytes per interleaved frame across all
    // channels, not per individual scalar sample — WebRTC's own
    // AudioTransportImpl asserts sizeof(int16_t) * nChannels ==
    // nBytesPerSample and aborts (fatal CHECK) if it doesn't hold.
    callback->RecordedDataIsAvailable(samples, frameCount, sizeof(int16_t) * channels, channels,
                                       static_cast<uint32_t>(sampleRateHz), /*totalDelayMS=*/0, /*clockDrift=*/0,
                                       /*currentMicLevel=*/0, /*keyPressed=*/false, newMicLevel);
}

int32_t CallAudioDeviceModule::RegisterAudioCallback(webrtc::AudioTransport* audioCallback) {
    const std::lock_guard<std::mutex> lock(transportMutex_);
    transport_ = audioCallback;
    return 0;
}

int32_t CallAudioDeviceModule::Init() {
    return 0;
}

int32_t CallAudioDeviceModule::PlayoutIsAvailable(bool* available) {
    *available = true;
    return 0;
}

int32_t CallAudioDeviceModule::InitPlayout() {
    playoutInitialized_.store(true);
    return 0;
}

bool CallAudioDeviceModule::PlayoutIsInitialized() const {
    return playoutInitialized_.load();
}

// WebRTC calls Start/StopPlayout serially from its own worker thread, never
// concurrently with each other — no extra locking around playoutThread_
// itself beyond the atomics governing its loop.
int32_t CallAudioDeviceModule::StartPlayout() {
    if (!playoutInitialized_.load() || playing_.load()) {
        return 0;
    }
    stopPlayoutThread_.store(false);
    playing_.store(true);
    playoutThread_ = std::thread(&CallAudioDeviceModule::playoutLoop, this);
    return 0;
}

int32_t CallAudioDeviceModule::StopPlayout() {
    stopPlayoutThread_.store(true);
    if (playoutThread_.joinable()) {
        playoutThread_.join();
    }
    playing_.store(false);
    return 0;
}

bool CallAudioDeviceModule::Playing() const {
    return playing_.load();
}

int32_t CallAudioDeviceModule::RecordingIsAvailable(bool* available) {
    *available = true;
    return 0;
}

int32_t CallAudioDeviceModule::InitRecording() {
    recordingInitialized_.store(true);
    return 0;
}

bool CallAudioDeviceModule::RecordingIsInitialized() const {
    return recordingInitialized_.load();
}

int32_t CallAudioDeviceModule::StartRecording() {
    recording_.store(true);
    return 0;
}

int32_t CallAudioDeviceModule::StopRecording() {
    recording_.store(false);
    return 0;
}

bool CallAudioDeviceModule::Recording() const {
    return recording_.load();
}

void CallAudioDeviceModule::playoutLoop() {
    // Fixed for the lifetime of one playout session, per setPlayoutFormat()'s
    // documented precondition (set before StartPlayout(), not touched again
    // until it stops).
    const int sampleRateHz = playoutSampleRateHz_;
    const size_t channels = playoutChannels_;
    const size_t framesPerBuffer = static_cast<size_t>(sampleRateHz) * kPlayoutIntervalMs / 1000;

    std::vector<int16_t> buffer(framesPerBuffer * channels);
    while (!stopPlayoutThread_.load()) {
        webrtc::AudioTransport* callback = transport();
        if (callback != nullptr) {
            size_t samplesOut = 0;
            int64_t elapsedTimeMs = 0;
            int64_t ntpTimeMs = 0;
            // Same nBytesPerSample-means-bytes-per-frame convention as
            // RecordedDataIsAvailable() above.
            const int32_t result = callback->NeedMorePlayData(framesPerBuffer, sizeof(int16_t) * channels, channels,
                                                                static_cast<uint32_t>(sampleRateHz), buffer.data(),
                                                                samplesOut, &elapsedTimeMs, &ntpTimeMs);
            if (result == 0 && samplesOut > 0 && playoutSink_) {
                playoutSink_(buffer.data(), samplesOut, sampleRateHz, channels);
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(kPlayoutIntervalMs));
    }
}

}  // namespace devicehub
