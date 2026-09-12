#include "devices/VoiceMessageRecorder.h"

#include "devices/audio_utils/WavEncoder.h"

namespace devicehub {

VoiceMessageRecorder::VoiceMessageRecorder(QObject* parent) : QObject(parent) {
    connect(&audioInput_, &AudioInputDevice::pcmDataAvailable, this,
            [this](const QByteArray& data, const QAudioFormat& format) {
                if (!recording_) {
                    return;
                }
                format_ = format;
                pcmBuffer_.append(data);
            });
    connect(&audioInput_, &AudioInputDevice::errorOccurred, this, &VoiceMessageRecorder::errorOccurred);
}

void VoiceMessageRecorder::start(const QAudioDevice& device) {
    pcmBuffer_.clear();
    recording_ = true;
    audioInput_.start(device);
}

QByteArray VoiceMessageRecorder::stop() {
    if (!recording_) {
        return {};
    }
    recording_ = false;
    audioInput_.stop();
    if (pcmBuffer_.isEmpty()) {
        return {};
    }
    return audio_utils::encodeWav(pcmBuffer_, format_);
}

bool VoiceMessageRecorder::isRecording() const {
    return recording_;
}

}  // namespace devicehub
