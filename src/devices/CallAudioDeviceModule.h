#pragma once

#include <modules/audio_device/include/audio_device_default.h>

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <thread>

namespace devicehub {

/**
 * @brief `webrtc::AudioDeviceModule` bridging WebRTC's audio pipeline to
 *        Qt Multimedia, instead of a real OS audio device — libwebrtc
 *        has no pluggable "push samples into an AudioSource" API, only
 *        this device-module interface (see issue #46, Phase 2).
 *
 * Capture: call pushCapturedAudio() from wherever local microphone PCM
 * becomes available (e.g. AudioInputDevice on the Qt GUI thread) —
 * forwarded straight to WebRTC's registered AudioTransport whenever
 * recording is active.
 *
 * Playout: WebRTC expects its AudioTransport::NeedMorePlayData() to be
 * pulled roughly every 10ms while playing — this class owns a
 * dedicated std::thread that does that pulling and forwards decoded
 * PCM to @p playoutSink. That callback runs on this class's own
 * thread, not the Qt GUI thread — a caller that touches Qt objects in
 * it is responsible for marshaling back (e.g.
 * QMetaObject::invokeMethod with Qt::QueuedConnection).
 *
 * Every other AudioDeviceModule method (device enumeration/selection,
 * volume, mute, ...) is inherited as a no-op from
 * webrtc_impl::AudioDeviceModuleDefault — this bridge has exactly one
 * fixed virtual device, selection doesn't apply.
 */
class CallAudioDeviceModule : public webrtc::webrtc_impl::AudioDeviceModuleDefault<webrtc::AudioDeviceModule> {
public:
    /// (samples, frameCount, sampleRateHz, channels) — samples points to
    /// frameCount * channels interleaved 16-bit PCM samples.
    using PlayoutSink = std::function<void(const int16_t* samples, size_t frameCount, int sampleRateHz,
                                            size_t channels)>;

    explicit CallAudioDeviceModule(PlayoutSink playoutSink);
    ~CallAudioDeviceModule() override;

    /// Overrides the format requested from WebRTC on the playout side
    /// (default 48000 Hz mono) — set this to match whatever real output
    /// device the playout sink actually writes to. Must be called
    /// before StartPlayout(); has no effect once the playout thread is
    /// already running.
    void setPlayoutFormat(int sampleRateHz, size_t channels);

    /// Overrides the total capture+render delay reported to WebRTC
    /// alongside every pushCapturedAudio() call (default 0). WebRTC's
    /// echo canceller uses this to time-align what it's currently
    /// rendering against what the microphone just captured — reporting
    /// 0 when there's real playout buffering (there is, see
    /// AudioOutputDevice::streamingBufferDurationMs()) makes the echo
    /// canceller worse than useless, not neutral. Safe to call from any
    /// thread; takes effect on the next pushCapturedAudio() call.
    void setTotalDelayMs(int delayMs);

    /// Pushes locally captured PCM into WebRTC. Safe to call from any
    /// thread; a no-op until recording has actually been started
    /// (StartRecording(), called by WebRTC once a send stream exists).
    void pushCapturedAudio(const int16_t* samples, size_t frameCount, int sampleRateHz, size_t channels);

    int32_t RegisterAudioCallback(webrtc::AudioTransport* audioCallback) override;
    int32_t Init() override;
    int32_t PlayoutIsAvailable(bool* available) override;
    int32_t InitPlayout() override;
    bool PlayoutIsInitialized() const override;
    int32_t StartPlayout() override;
    int32_t StopPlayout() override;
    bool Playing() const override;
    int32_t RecordingIsAvailable(bool* available) override;
    int32_t InitRecording() override;
    bool RecordingIsInitialized() const override;
    int32_t StartRecording() override;
    int32_t StopRecording() override;
    bool Recording() const override;

private:
    void playoutLoop();
    [[nodiscard]] webrtc::AudioTransport* transport() const;

    PlayoutSink playoutSink_;

    mutable std::mutex transportMutex_;
    webrtc::AudioTransport* transport_ = nullptr;

    std::atomic<bool> playoutInitialized_{false};
    std::atomic<bool> recordingInitialized_{false};
    std::atomic<bool> playing_{false};
    std::atomic<bool> recording_{false};

    std::thread playoutThread_;
    std::atomic<bool> stopPlayoutThread_{false};

    int playoutSampleRateHz_ = 48000;
    size_t playoutChannels_ = 1;
    std::atomic<int> totalDelayMs_{0};
};

}  // namespace devicehub
