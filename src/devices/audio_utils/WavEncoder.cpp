#include "devices/audio_utils/WavEncoder.h"

#include <QDataStream>
#include <QIODevice>

namespace devicehub::audio_utils {

namespace {
constexpr quint16 kPcmAudioFormat = 1;
constexpr quint32 kFmtSubchunkSize = 16;  // 16 для несжатого PCM, без расширения
constexpr int kHeaderSizeBeforeData = 44;
}  // namespace

QByteArray encodeWav(const QByteArray& pcmData, const QAudioFormat& format) {
    const auto sampleRate = static_cast<quint32>(format.sampleRate());
    const auto numChannels = static_cast<quint16>(format.channelCount());
    const auto bitsPerSample = static_cast<quint16>(format.bytesPerSample() * 8);
    const auto blockAlign = static_cast<quint16>(format.bytesPerFrame());
    const quint32 byteRate = sampleRate * blockAlign;
    const auto dataSize = static_cast<quint32>(pcmData.size());
    const quint32 riffChunkSize = static_cast<quint32>(kHeaderSizeBeforeData) - 8 + dataSize;

    QByteArray wav;
    wav.reserve(kHeaderSizeBeforeData + pcmData.size());
    QDataStream stream(&wav, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);

    stream.writeRawData("RIFF", 4);
    stream << riffChunkSize;
    stream.writeRawData("WAVE", 4);
    stream.writeRawData("fmt ", 4);
    stream << kFmtSubchunkSize;
    stream << kPcmAudioFormat;
    stream << numChannels;
    stream << sampleRate;
    stream << byteRate;
    stream << blockAlign;
    stream << bitsPerSample;
    stream.writeRawData("data", 4);
    stream << dataSize;
    stream.writeRawData(pcmData.constData(), static_cast<int>(pcmData.size()));

    return wav;
}

}  // namespace devicehub::audio_utils
