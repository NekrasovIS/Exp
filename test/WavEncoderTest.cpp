#include "devices/audio_utils/WavEncoder.h"

#include <gtest/gtest.h>

#include <QAudioFormat>
#include <QByteArray>
#include <QDataStream>
#include <QIODevice>
#include <cstdint>

namespace devicehub::audio_utils {
namespace {

QAudioFormat makeFormat(int sampleRate, int channelCount) {
    QAudioFormat format;
    format.setSampleRate(sampleRate);
    format.setChannelCount(channelCount);
    format.setSampleFormat(QAudioFormat::Int16);
    return format;
}

TEST(WavEncoderTest, PrependsA44ByteHeaderBeforeThePcmData) {
    const QByteArray pcm(100, '\x7f');

    const QByteArray wav = encodeWav(pcm, makeFormat(48000, 1));

    ASSERT_EQ(wav.size(), 44 + pcm.size());
    EXPECT_EQ(wav.right(pcm.size()), pcm);
}

TEST(WavEncoderTest, WritesRiffAndWaveAndFmtAndDataChunkIds) {
    const QByteArray wav = encodeWav(QByteArray(10, '\0'), makeFormat(48000, 1));

    EXPECT_EQ(wav.mid(0, 4), QByteArray("RIFF"));
    EXPECT_EQ(wav.mid(8, 4), QByteArray("WAVE"));
    EXPECT_EQ(wav.mid(12, 4), QByteArray("fmt "));
    EXPECT_EQ(wav.mid(36, 4), QByteArray("data"));
}

TEST(WavEncoderTest, EncodesSampleRateChannelCountAndBitsPerSampleFromTheFormat) {
    const QByteArray wav = encodeWav(QByteArray(10, '\0'), makeFormat(44100, 2));

    QDataStream stream(wav);
    stream.setByteOrder(QDataStream::LittleEndian);
    stream.skipRawData(20);  // "RIFF" + size + "WAVE" + "fmt " + subchunk1Size

    quint16 audioFormat = 0;
    quint16 numChannels = 0;
    quint32 sampleRate = 0;
    quint32 byteRate = 0;
    quint16 blockAlign = 0;
    quint16 bitsPerSample = 0;
    stream >> audioFormat >> numChannels >> sampleRate >> byteRate >> blockAlign >> bitsPerSample;

    EXPECT_EQ(audioFormat, 1);  // PCM, без сжатия
    EXPECT_EQ(numChannels, 2);
    EXPECT_EQ(sampleRate, 44100u);
    EXPECT_EQ(bitsPerSample, 16);
    EXPECT_EQ(blockAlign, 4);  // 2 канала * 2 байта на сэмпл
    EXPECT_EQ(byteRate, 44100u * 4);
}

TEST(WavEncoderTest, EncodesDataChunkSizeAsTheExactPcmByteCount) {
    const QByteArray pcm(1234, '\x01');

    const QByteArray wav = encodeWav(pcm, makeFormat(16000, 1));

    QDataStream stream(wav);
    stream.setByteOrder(QDataStream::LittleEndian);
    stream.skipRawData(40);  // всё до поля размера data-чанка
    quint32 dataChunkSize = 0;
    stream >> dataChunkSize;

    EXPECT_EQ(dataChunkSize, static_cast<quint32>(pcm.size()));
}

TEST(WavEncoderTest, EncodesRiffChunkSizeAsFileSizeMinusEightBytes) {
    const QByteArray pcm(500, '\0');

    const QByteArray wav = encodeWav(pcm, makeFormat(48000, 1));

    QDataStream stream(wav);
    stream.setByteOrder(QDataStream::LittleEndian);
    stream.skipRawData(4);  // "RIFF"
    quint32 riffChunkSize = 0;
    stream >> riffChunkSize;

    EXPECT_EQ(riffChunkSize, static_cast<quint32>(wav.size() - 8));
}

TEST(WavEncoderTest, HandlesEmptyPcmDataWithoutCrashing) {
    const QByteArray wav = encodeWav(QByteArray(), makeFormat(48000, 1));

    EXPECT_EQ(wav.size(), 44);
}

}  // namespace
}  // namespace devicehub::audio_utils
