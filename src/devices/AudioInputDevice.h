#pragma once

#include <QAudioDevice>
#include <QAudioFormat>
#include <QObject>
#include <memory>

class QAudioSource;
class QIODevice;

namespace devicehub {

/**
 * @brief Captures audio from a microphone and reports its input level.
 *
 * Wraps QAudioSource; reads raw PCM as it arrives and emits levelChanged()
 * with a normalized [0, 1] amplitude so the UI can drive a level meter,
 * plus pcmDataAvailable() with the raw buffer for consumers that need
 * actual samples (e.g. CallManager). One QAudioSource at a time — a
 * second start() (from either the settings mic test or a call) stops
 * whatever was capturing before it.
 */
class AudioInputDevice : public QObject {
    Q_OBJECT

public:
    explicit AudioInputDevice(QObject* parent = nullptr);
    ~AudioInputDevice() override;

    /// Starts capturing from @p device.
    void start(const QAudioDevice& device);

    /// Stops capturing if in progress.
    void stop();

    /// @return True while actively capturing.
    [[nodiscard]] bool isCapturing() const;

signals:
    /// Emitted as new audio arrives, level normalized to [0, 1].
    void levelChanged(float level);

    /// Emitted alongside levelChanged(), same raw buffer — for
    /// consumers that need actual samples (e.g. CallManager forwarding
    /// captured audio into a call), not just a level meter.
    void pcmDataAvailable(const QByteArray& data, const QAudioFormat& format);

    /// Emitted when capture can't start (including permission denial).
    void errorOccurred(const QString& message);

private slots:
    void readAvailableData();

private:
    void startCapture(const QAudioDevice& device);
    [[nodiscard]] static float computeLevel(const QByteArray& pcmData, const QAudioFormat& format);

    std::unique_ptr<QAudioSource> source_;
    QIODevice* stream_ = nullptr;
    QAudioFormat format_;
};

}  // namespace devicehub
