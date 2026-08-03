#include "devices/AudioInputDevice.h"

#include <QAudioSource>
#include <QIODevice>
#include <limits>

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace devicehub {

AudioInputDevice::AudioInputDevice(QObject* parent) : QObject(parent) {}

AudioInputDevice::~AudioInputDevice() = default;

void AudioInputDevice::start(const QAudioDevice& device) {
    stop();

    format_ = device.preferredFormat();
    format_.setSampleFormat(QAudioFormat::Int16);

    source_ = std::make_unique<QAudioSource>(device, format_);
    stream_ = source_->start();
    if (stream_ != nullptr) {
        connect(stream_, &QIODevice::readyRead, this, &AudioInputDevice::readAvailableData);
    }
}

void AudioInputDevice::stop() {
    if (source_) {
        source_->stop();
        source_.reset();
    }
    stream_ = nullptr;
}

bool AudioInputDevice::isCapturing() const {
    return source_ && source_->state() == QAudio::ActiveState;
}

void AudioInputDevice::readAvailableData() {
    if (stream_ == nullptr) {
        return;
    }
    const QByteArray pcmData = stream_->readAll();
    if (pcmData.isEmpty()) {
        return;
    }
    emit levelChanged(computeLevel(pcmData, format_));
}

float AudioInputDevice::computeLevel(const QByteArray& pcmData, const QAudioFormat& format) {
    if (format.sampleFormat() != QAudioFormat::Int16 || pcmData.size() < 2) {
        return 0.0f;
    }

    const auto* samples = reinterpret_cast<const int16_t*>(pcmData.constData());
    const qsizetype sampleCount = pcmData.size() / static_cast<qsizetype>(sizeof(int16_t));

    int64_t sumOfSquares = 0;
    for (qsizetype i = 0; i < sampleCount; ++i) {
        sumOfSquares += static_cast<int64_t>(samples[i]) * samples[i];
    }
    const double rms = std::sqrt(static_cast<double>(sumOfSquares) / static_cast<double>(sampleCount));
    const double normalized = rms / static_cast<double>(std::numeric_limits<int16_t>::max());
    return static_cast<float>(std::clamp(normalized, 0.0, 1.0));
}

}  // namespace devicehub
