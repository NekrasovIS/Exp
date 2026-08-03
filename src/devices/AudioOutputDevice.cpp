#include "devices/AudioOutputDevice.h"

#include <QAudioSink>
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
        const float sample = static_cast<float>(qSin(2.0 * M_PI * frequencyHz * t));
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
        if (state == QAudio::IdleState || state == QAudio::StoppedState) {
            emit finished();
        }
    });
    sink_->start(&toneBuffer_);
}

void AudioOutputDevice::stop() {
    if (sink_) {
        sink_->stop();
        sink_.reset();
    }
    toneBuffer_.close();
}

bool AudioOutputDevice::isPlaying() const {
    return sink_ && sink_->state() == QAudio::ActiveState;
}

}  // namespace devicehub
