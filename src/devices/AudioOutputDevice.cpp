#include "devices/AudioOutputDevice.h"

#include <QAudioSink>
#include <QIODevice>
#include <QtMath>

#include <cstdint>
#include <limits>

namespace devicehub {

namespace {
constexpr int kSampleRate = 44100;
constexpr double kToneDurationSeconds = 1.5;
}  // namespace

AudioOutputDevice::AudioOutputDevice(QObject* parent) : QObject(parent) {}

AudioOutputDevice::~AudioOutputDevice() = default;

QByteArray AudioOutputDevice::generateSineWave(const QAudioFormat& format, double frequencyHz,
                                                double durationSeconds) const {
    const qsizetype sampleCount = static_cast<qsizetype>(format.sampleRate() * durationSeconds);
    QByteArray data;
    data.reserve(sampleCount * format.bytesPerFrame());

    for (qsizetype i = 0; i < sampleCount; ++i) {
        const double t = static_cast<double>(i) / format.sampleRate();
        const double sample = qSin(2.0 * M_PI * frequencyHz * t);
        for (int channel = 0; channel < format.channelCount(); ++channel) {
            const int32_t pcm = static_cast<int32_t>(sample * 0.5 * std::numeric_limits<int16_t>::max());
            const int16_t pcm16 = static_cast<int16_t>(pcm);
            data.append(reinterpret_cast<const char*>(&pcm16), sizeof(pcm16));
        }
    }
    return data;
}

void AudioOutputDevice::playTestTone(const QAudioDevice& device, double frequencyHz) {
    stop();

    QAudioFormat format;
    format.setSampleRate(kSampleRate);
    format.setChannelCount(2);
    format.setSampleFormat(QAudioFormat::Int16);

    if (!device.isFormatSupported(format)) {
        format = device.preferredFormat();
    }

    toneData_ = generateSineWave(format, frequencyHz, kToneDurationSeconds);
    toneBuffer_.setBuffer(&toneData_);
    toneBuffer_.open(QIODevice::ReadOnly);

    sink_ = std::make_unique<QAudioSink>(device, format);
    connect(sink_.get(), &QAudioSink::stateChanged, this, [this](QAudio::State state) {
        if (!streaming_ && (state == QAudio::IdleState || state == QAudio::StoppedState)) {
            emit finished();
        }
    });
    sink_->start(&toneBuffer_);
}

bool AudioOutputDevice::startStreaming(const QAudioDevice& device, const QAudioFormat& format) {
    stop();

    if (!device.isFormatSupported(format)) {
        emit errorOccurred(tr("Output device doesn't support the requested format"));
        return false;
    }

    streaming_ = true;
    sink_ = std::make_unique<QAudioSink>(device, format);
    sink_->setBufferSize(format.bytesForDuration(qint64{streamingBufferDurationMs()} * 1000));
    connect(sink_.get(), &QAudioSink::stateChanged, this, [this](QAudio::State state) {
        if (!streaming_ && (state == QAudio::IdleState || state == QAudio::StoppedState)) {
            emit finished();
        }
    });
    pushStream_ = sink_->start();
    return pushStream_ != nullptr;
}

void AudioOutputDevice::writeAudio(const QByteArray& pcm) {
    if (pushStream_ != nullptr) {
        pushStream_->write(pcm);
    }
}

void AudioOutputDevice::stop() {
    if (sink_) {
        sink_->stop();
        sink_.reset();
    }
    pushStream_ = nullptr;
    streaming_ = false;
    toneBuffer_.close();
}

bool AudioOutputDevice::isPlaying() const {
    return sink_ && sink_->state() == QAudio::ActiveState;
}

}  // namespace devicehub
