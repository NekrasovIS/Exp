#pragma once

#include <QAudioDevice>
#include <QAudioFormat>
#include <QObject>
#include <memory>

class QAudioSource;
class QIODevice;

namespace devicehub {

/**
 * @brief Захватывает аудио с микрофона и сообщает его уровень входного
 *        сигнала.
 *
 * Оборачивает QAudioSource; читает сырой PCM по мере поступления и
 * испускает levelChanged() с нормализованной амплитудой [0, 1], чтобы UI
 * мог управлять индикатором уровня, а также pcmDataAvailable() с сырым
 * буфером для потребителей, которым нужны настоящие сэмплы (например,
 * CallManager). Одновременно только один QAudioSource — второй вызов
 * start() (будь то из mic-теста в настройках или из звонка) останавливает
 * то, что захватывалось до него.
 */
class AudioInputDevice : public QObject {
    Q_OBJECT

public:
    explicit AudioInputDevice(QObject* parent = nullptr);
    ~AudioInputDevice() override;

    /// Начинает захват с @p device.
    void start(const QAudioDevice& device);

    /// Останавливает захват, если он выполняется.
    void stop();

    /// @return True, пока идёт активный захват.
    [[nodiscard]] bool isCapturing() const;

signals:
    /// Испускается по мере поступления нового аудио, уровень
    /// нормализован в [0, 1].
    void levelChanged(float level);

    /// Испускается вместе с levelChanged(), тот же сырой буфер — для
    /// потребителей, которым нужны настоящие сэмплы (например,
    /// CallManager, пересылающий захваченное аудио в звонок), а не
    /// просто индикатор уровня.
    void pcmDataAvailable(const QByteArray& data, const QAudioFormat& format);

    /// Испускается, когда захват не может начаться (включая отказ в
    /// разрешении).
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
