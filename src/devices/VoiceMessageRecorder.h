#pragma once

#include <QAudioDevice>
#include <QAudioFormat>
#include <QByteArray>
#include <QObject>

#include "devices/AudioInputDevice.h"

namespace devicehub {

/**
 * @brief Записывает короткое голосовое сообщение с микрофона в WAV
 *        (issue #359).
 *
 * Владеет собственным `AudioInputDevice`, отдельным от общего, которым
 * пользуются mic-тест в настройках (`MainWindow::audioInput_`) и
 * `CallManager` во время звонка — запись голосового сообщения не
 * конфликтует ни с одним из них: каждый открывает микрофон своим
 * `QAudioSource` независимо на уровне Qt/ОС. Накапливает сырой PCM по
 * мере поступления (`AudioInputDevice::pcmDataAvailable()`, тот же
 * сигнал, что уже использует `CallManager`), на stop() отдаёт готовый
 * WAV-файл через `audio_utils::encodeWav()`.
 */
class VoiceMessageRecorder : public QObject {
    Q_OBJECT

public:
    explicit VoiceMessageRecorder(QObject* parent = nullptr);

    /// Начинает запись с @p device. Повторный вызов, пока уже идёт
    /// запись, перезапускает её с нуля — тот же принцип, что и у
    /// `AudioInputDevice::start()`, которую он оборачивает.
    void start(const QAudioDevice& device);

    /// Останавливает запись и возвращает готовый WAV-файл; пустой
    /// `QByteArray`, если запись не была начата или не поступило ни
    /// одного сэмпла (например, разрешение на микрофон было отклонено).
    [[nodiscard]] QByteArray stop();

    /// @return True, пока идёт активная запись.
    [[nodiscard]] bool isRecording() const;

signals:
    /// Запись не может начаться (включая отказ в разрешении на
    /// микрофон) — тот же сигнал `AudioInputDevice`, проброшенный
    /// наружу без изменений.
    void errorOccurred(const QString& message);

private:
    AudioInputDevice audioInput_;
    QByteArray pcmBuffer_;
    QAudioFormat format_;
    bool recording_ = false;
};

}  // namespace devicehub
