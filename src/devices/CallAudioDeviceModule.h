#pragma once

#include <modules/audio_device/include/audio_device_default.h>

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <stop_token>
#include <thread>

namespace devicehub {

/**
 * @brief `webrtc::AudioDeviceModule`, соединяющий аудио-конвейер WebRTC
 *        с Qt Multimedia вместо настоящего аудиоустройства ОС —
 *        у libwebrtc нет подключаемого API вида «протолкнуть сэмплы в
 *        AudioSource», только этот интерфейс device-module (см. issue
 *        #46, Phase 2).
 *
 * Захват: вызывайте pushCapturedAudio() оттуда, где становится доступен
 * PCM с локального микрофона (например, AudioInputDevice на GUI-потоке
 * Qt) — пересылается напрямую в зарегистрированный AudioTransport
 * WebRTC, пока активна запись.
 *
 * Воспроизведение: WebRTC ожидает, что его
 * AudioTransport::NeedMorePlayData() будет опрашиваться примерно каждые
 * 10 мс во время воспроизведения — этот класс владеет отдельным
 * std::jthread, который выполняет этот опрос и пересылает
 * декодированный PCM в @p playoutSink. Этот колбэк выполняется на
 * собственном потоке этого класса, а не на GUI-потоке Qt — вызывающий
 * код, трогающий в нём объекты Qt, сам отвечает за маршализацию обратно
 * (например, через QMetaObject::invokeMethod с Qt::QueuedConnection).
 *
 * Любой другой метод AudioDeviceModule (перечисление/выбор устройств,
 * громкость, mute, ...) унаследован как no-op от
 * webrtc_impl::AudioDeviceModuleDefault — у этого моста ровно одно
 * фиксированное виртуальное устройство, выбор здесь неприменим.
 */
class CallAudioDeviceModule : public webrtc::webrtc_impl::AudioDeviceModuleDefault<webrtc::AudioDeviceModule> {
public:
    /// (samples, frameCount, sampleRateHz, channels) — samples указывает
    /// на frameCount * channels чередующихся 16-битных PCM-сэмплов.
    using PlayoutSink = std::function<void(const int16_t* samples, size_t frameCount, int sampleRateHz,
                                            size_t channels)>;

    explicit CallAudioDeviceModule(PlayoutSink playoutSink);
    ~CallAudioDeviceModule() override;

    /// Переопределяет формат, запрашиваемый у WebRTC на стороне
    /// воспроизведения (по умолчанию 48000 Гц моно) — устанавливайте
    /// его в соответствии с тем, во что реально пишет playout sink на
    /// настоящем устройстве вывода. Должен вызываться до StartPlayout();
    /// не имеет эффекта, если поток воспроизведения уже запущен.
    void setPlayoutFormat(int sampleRateHz, size_t channels);

    /// Переопределяет суммарную задержку захвата+рендеринга,
    /// сообщаемую в WebRTC при каждом вызове pushCapturedAudio() (по
    /// умолчанию 0). Эхоподавитель WebRTC использует это значение, чтобы
    /// синхронизировать по времени то, что он сейчас рендерит, с тем,
    /// что микрофон только что захватил — сообщение 0, когда реальная
    /// буферизация воспроизведения есть (а она есть, см.
    /// AudioOutputDevice::streamingBufferDurationMs()), делает
    /// эхоподавитель хуже, чем бесполезным, а не нейтральным. Безопасно
    /// вызывать из любого потока; вступает в силу при следующем вызове
    /// pushCapturedAudio().
    void setTotalDelayMs(int delayMs);

    /// Проталкивает локально захваченный PCM в WebRTC. Безопасно
    /// вызывать из любого потока; ничего не делает, пока запись
    /// фактически не запущена (StartRecording(), вызывается WebRTC как
    /// только появляется отправляющий поток).
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
    void playoutLoop(std::stop_token stopToken);
    [[nodiscard]] webrtc::AudioTransport* transport() const;

    PlayoutSink playoutSink_;

    mutable std::mutex transportMutex_;
    webrtc::AudioTransport* transport_ = nullptr;

    std::atomic<bool> playoutInitialized_{false};
    std::atomic<bool> recordingInitialized_{false};
    std::atomic<bool> playing_{false};
    std::atomic<bool> recording_{false};

    /// jthread, а не thread (C++20) — сам запрашивает остановку и
    /// join'ится при разрушении/переприсваивании, самодельный атомарный
    /// флаг остановки не нужен.
    std::jthread playoutThread_;

    int playoutSampleRateHz_ = 48000;
    size_t playoutChannels_ = 1;
    std::atomic<int> totalDelayMs_{0};
};

}  // namespace devicehub
