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

void CallAudioDeviceModule::setTotalDelayMs(int delayMs) {
    totalDelayMs_.store(delayMs);
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
    // nBytesPerSample здесь означает байты на чередующийся кадр по всем
    // каналам, а не на отдельный скалярный сэмпл — собственный
    // AudioTransportImpl у WebRTC проверяет условие sizeof(int16_t) *
    // nChannels == nBytesPerSample и аварийно завершается (фатальный
    // CHECK), если оно не выполняется.
    callback->RecordedDataIsAvailable(samples, frameCount, sizeof(int16_t) * channels, channels,
                                       static_cast<uint32_t>(sampleRateHz),
                                       static_cast<uint32_t>(totalDelayMs_.load()), /*clockDrift=*/0,
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

// WebRTC вызывает Start/StopPlayout последовательно на своём собственном
// worker-потоке, никогда не параллельно друг с другом — дополнительная
// блокировка вокруг самого playoutThread_ не нужна сверх атомиков,
// управляющих его циклом.
int32_t CallAudioDeviceModule::StartPlayout() {
    if (!playoutInitialized_.load() || playing_.load()) {
        return 0;
    }
    playing_.store(true);
    // Не std::jthread(&CallAudioDeviceModule::playoutLoop, this):
    // jthread вставляет stop_token как аргумент сразу после callable,
    // что для указателя на метод класса попадает в слот *объекта*, а не
    // после него — молча откатывается к вызову playoutLoop() вовсе без
    // stop_token (что здесь некорректно, поскольку теперь метод его
    // принимает). Лямбда полностью обходит эту неоднозначность.
    playoutThread_ = std::jthread([this](std::stop_token stopToken) { playoutLoop(std::move(stopToken)); });
    return 0;
}

int32_t CallAudioDeviceModule::StopPlayout() {
    playoutThread_.request_stop();
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

void CallAudioDeviceModule::playoutLoop(std::stop_token stopToken) {
    // Фиксировано на всё время жизни одной сессии воспроизведения,
    // согласно задокументированному предусловию setPlayoutFormat()
    // (устанавливается до StartPlayout(), больше не трогается до
    // остановки).
    const int sampleRateHz = playoutSampleRateHz_;
    const size_t channels = playoutChannels_;
    const size_t framesPerBuffer = static_cast<size_t>(sampleRateHz) * kPlayoutIntervalMs / 1000;

    std::vector<int16_t> buffer(framesPerBuffer * channels);
    // sleep_until относительно якоря с фиксированным шагом вместо
    // sleep_for(10ms) после каждой итерации — sleep_for позволяет
    // стоимости работы текущей итерации (плюс собственной задержке
    // пробуждения ОС) каждый раз добавляться поверх следующего
    // 10-миллисекундного ожидания, из-за чего цикл постепенно отстаёт
    // от реального времени при любой устойчивой нагрузке на систему и
    // со временем морит playout sink голодом (слышно как
    // потрескивание), а не просто изредка дрожит, что мог бы поглотить
    // больший буфер sink'а.
    auto nextTick = std::chrono::steady_clock::now();
    while (!stopToken.stop_requested()) {
        nextTick += std::chrono::milliseconds(kPlayoutIntervalMs);
        webrtc::AudioTransport* callback = transport();
        if (callback != nullptr) {
            size_t samplesOut = 0;
            int64_t elapsedTimeMs = 0;
            int64_t ntpTimeMs = 0;
            // Та же конвенция «nBytesPerSample означает байты на кадр»,
            // что и у RecordedDataIsAvailable() выше.
            const int32_t result = callback->NeedMorePlayData(framesPerBuffer, sizeof(int16_t) * channels, channels,
                                                                static_cast<uint32_t>(sampleRateHz), buffer.data(),
                                                                samplesOut, &elapsedTimeMs, &ntpTimeMs);
            if (result == 0 && samplesOut > 0 && playoutSink_) {
                playoutSink_(buffer.data(), samplesOut, sampleRateHz, channels);
            }
        }
        std::this_thread::sleep_until(nextTick);
    }
}

}  // namespace devicehub
