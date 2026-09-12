#pragma once

#include <QAudioFormat>
#include <QByteArray>

namespace devicehub::audio_utils {

/// @file Оборачивает сырой PCM в валидный WAV-файл (issue #359) — общая
/// утилита для любого будущего потребителя, которому нужно превратить
/// поток `AudioInputDevice::pcmDataAvailable()` в файл, а не только для
/// голосовых сообщений (`VoiceMessageRecorder`), поэтому вынесена
/// отдельно, а не как приватный метод одного класса.

/// Оборачивает @p pcmData (как его отдаёт
/// `AudioInputDevice::pcmDataAvailable()`, всегда `QAudioFormat::Int16`)
/// в валидный WAV-файл — 44-байтный canonical RIFF/WAVE-заголовок,
/// построенный из @p format (частота дискретизации, число каналов,
/// байт на сэмпл), затем сами сэмплы без изменений. Результат готов и к
/// загрузке как обычное вложение чата (issue #116), и к воспроизведению
/// любым плеером, включая `QMediaPlayer`.
[[nodiscard]] QByteArray encodeWav(const QByteArray& pcmData, const QAudioFormat& format);

}  // namespace devicehub::audio_utils
