#pragma once

#include <QAudioDevice>
#include <QAudioFormat>
#include <QBuffer>
#include <QByteArray>
#include <QObject>
#include <memory>

class QAudioSink;

namespace devicehub {

/**
 * @brief Plays a test tone through a chosen audio output device.
 *
 * Used to verify a speaker/headphone device is reachable and working
 * without needing a real media file. Generates a short sine wave in
 * memory and streams it through QAudioSink.
 */
class AudioOutputDevice : public QObject {
    Q_OBJECT

public:
    explicit AudioOutputDevice(QObject* parent = nullptr);
    ~AudioOutputDevice() override;

    /// Starts playing a sine test tone on @p device at @p frequencyHz.
    void playTestTone(const QAudioDevice& device, double frequencyHz = 440.0);

    /// Stops playback if in progress.
    void stop();

    /// @return True while a test tone is actively playing.
    [[nodiscard]] bool isPlaying() const;

signals:
    /// Emitted when playback finishes (buffer drained or stopped).
    void finished();

private:
    QByteArray generateSineWave(const QAudioFormat& format, double frequencyHz, double durationSeconds) const;

    std::unique_ptr<QAudioSink> sink_;
    QByteArray toneData_;
    QBuffer toneBuffer_;
};

}  // namespace devicehub
