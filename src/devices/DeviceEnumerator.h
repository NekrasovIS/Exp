#pragma once

#include <QAudioDevice>
#include <QCameraDevice>
#include <QList>

class QScreen;

namespace devicehub {

/**
 * @brief Перечисляет устройства захвата/воспроизведения аудио и видео,
 *        доступные в системе на данный момент.
 *
 * Тонкая обёртка вокруг QMediaDevices, чтобы остальной код зависел от
 * единого, тестируемого шва, а не напрямую от глобального API устройств
 * Qt.
 */
class DeviceEnumerator {
public:
    /// @return Все доступные устройства аудиовывода (динамики/наушники).
    [[nodiscard]] QList<QAudioDevice> audioOutputs() const;

    /// @return Все доступные устройства аудиоввода (микрофоны).
    [[nodiscard]] QList<QAudioDevice> audioInputs() const;

    /// @return Все доступные устройства камер.
    [[nodiscard]] QList<QCameraDevice> cameras() const;

    /// @return Все доступные экраны (мониторы), которые можно захватить.
    [[nodiscard]] QList<QScreen*> screens() const;
};

}  // namespace devicehub
