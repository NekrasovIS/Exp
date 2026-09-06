#pragma once

#include <QAudioDevice>
#include <QAudioFormat>
#include <QBuffer>
#include <QByteArray>
#include <QObject>
#include <memory>

class QAudioSink;
class QIODevice;

namespace devicehub {

/**
 * @brief Воспроизводит тестовый тон или потоковый PCM (например, живой
 *        звонок) через выбранное устройство аудиовывода.
 *
 * playTestTone() генерирует короткую синусоиду в памяти и стримит её
 * через QAudioSink. startStreaming()/writeAudio() вместо этого переводят
 * sink в режим push для вызывающего кода (например, CallManager), у
 * которого есть собственный непрерывный PCM для доставки, без
 * фиксированного конца. Одновременно только один QAudioSink — запуск
 * любого из них останавливает то, что воспроизводилось до него.
 */
class AudioOutputDevice : public QObject {
    Q_OBJECT

public:
    explicit AudioOutputDevice(QObject* parent = nullptr);
    ~AudioOutputDevice() override;

    /// Начинает воспроизведение синусоидального тестового тона на
    /// @p device на частоте @p frequencyHz.
    void playTestTone(const QAudioDevice& device, double frequencyHz = 440.0);

    /// Начинает воспроизведение в режиме push на @p device для
    /// потокового аудио, доставляемого через writeAudio() по мере
    /// поступления, вместо фиксированного буфера в памяти. @p format
    /// уже должен быть ровно тем, что будет записывать вызывающий код —
    /// ресемплинг здесь не выполняется. Возвращает false (и испускает
    /// errorOccurred()), если @p device его не поддерживает. Использует
    /// буфер QAudioSink больше стандартного (~500 мс), поскольку
    /// вызывающий код (например, CallManager) доставляет чанки из
    /// другого потока через межпотоковый вызов через очередь, а не через
    /// плотный низколатентный колбэк — без этого запаса дрожание
    /// планирования GUI event-loop приводит к слышимому потрескиванию
    /// от underrun.
    bool startStreaming(const QAudioDevice& device, const QAudioFormat& format);

    /// Записывает @p pcm в sink, запущенный startStreaming(). Ничего не
    /// делает, если стриминг не был запущен.
    void writeAudio(const QByteArray& pcm);

    /// Останавливает воспроизведение, если оно выполняется.
    void stop();

    /// @return True, пока активно воспроизводится тестовый тон или поток.
    [[nodiscard]] bool isPlaying() const;

    /// Длительность буфера, под которую startStreaming() подгоняет
    /// размер своего QAudioSink. Потоковое аудио (живой звонок)
    /// приходит небольшими чанками из другого потока, проходя через
    /// межпотоковый вызов через очередь, прежде чем достичь
    /// writeAudio() — в отличие от плотного низколатентного колбэка
    /// воспроизведения, этот переход подвержен дрожанию планирования
    /// GUI event-loop Qt, а сам питающий поток — это std::jthread с
    /// обычным приоритетом без гарантий планирования реального времени.
    /// Стандартный буфер самого QAudioSink рассчитан на низколатентный
    /// случай и легко переживает underrun под таким дрожанием, слышимый
    /// как потрескивание; это значение щедрое, но голосовой звонок
    /// переносит добавленную задержку намного лучше, чем слышимые
    /// артефакты. Открыто наружу, чтобы вызывающий код, который также
    /// питает эхоподавитель WebRTC (например, CallManager через
    /// CallAudioDeviceModule::setTotalDelayMs()), мог сообщать точную
    /// задержку воспроизведения вместо угадывания.
    [[nodiscard]] static constexpr int streamingBufferDurationMs() { return 500; }

signals:
    /// Испускается, когда воспроизведение тестового тона завершается
    /// (буфер исчерпан или остановлен) — не испускается для сессий
    /// startStreaming(), где простаивающий между записями sink —
    /// нормальное состояние, а не «завершено».
    void finished();

    void errorOccurred(const QString& message);

private:
    QByteArray generateSineWave(const QAudioFormat& format, double frequencyHz, double durationSeconds) const;

    std::unique_ptr<QAudioSink> sink_;
    QByteArray toneData_;
    QBuffer toneBuffer_;
    QIODevice* pushStream_ = nullptr;
    bool streaming_ = false;
};

}  // namespace devicehub
