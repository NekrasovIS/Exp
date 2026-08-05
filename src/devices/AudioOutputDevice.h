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
 * @brief Plays a test tone, or streamed PCM (e.g. a live call), through
 *        a chosen audio output device.
 *
 * playTestTone() generates a short sine wave in memory and streams it
 * through QAudioSink. startStreaming()/writeAudio() instead put the
 * sink in push mode for a caller (e.g. CallManager) that has its own
 * ongoing PCM to deliver, with no fixed end. One QAudioSink at a time —
 * starting either stops whatever was playing before it.
 */
class AudioOutputDevice : public QObject {
    Q_OBJECT

public:
    explicit AudioOutputDevice(QObject* parent = nullptr);
    ~AudioOutputDevice() override;

    /// Starts playing a sine test tone on @p device at @p frequencyHz.
    void playTestTone(const QAudioDevice& device, double frequencyHz = 440.0);

    /// Starts push-mode playback on @p device for streaming audio
    /// delivered via writeAudio() as it arrives, instead of a fixed
    /// in-memory buffer. @p format must already be exactly what the
    /// caller will write — no resampling happens here. Returns false
    /// (and emits errorOccurred()) if @p device doesn't support it.
    /// Uses a larger-than-default QAudioSink buffer (~200ms) since the
    /// caller (e.g. CallManager) delivers chunks from another thread
    /// via a queued cross-thread call, not a tight low-latency
    /// callback — without the extra headroom, GUI event-loop
    /// scheduling jitter causes audible underrun crackling.
    bool startStreaming(const QAudioDevice& device, const QAudioFormat& format);

    /// Writes @p pcm to the sink started by startStreaming(). No-op if
    /// streaming hasn't been started.
    void writeAudio(const QByteArray& pcm);

    /// Stops playback if in progress.
    void stop();

    /// @return True while a test tone or a stream is actively playing.
    [[nodiscard]] bool isPlaying() const;

signals:
    /// Emitted when test-tone playback finishes (buffer drained or
    /// stopped) — not emitted for startStreaming() sessions, where an
    /// idle sink between writes is normal, not "done".
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
