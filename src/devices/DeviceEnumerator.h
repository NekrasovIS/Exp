#pragma once

#include <QAudioDevice>
#include <QCameraDevice>
#include <QList>

class QScreen;

namespace devicehub {

/**
 * @brief Lists the audio and video capture/playback devices currently
 *        available on the system.
 *
 * Thin wrapper around QMediaDevices so the rest of the codebase depends on
 * a single, testable seam instead of Qt's global device API directly.
 */
class DeviceEnumerator {
public:
    /// @return All available audio output (speaker/headphone) devices.
    [[nodiscard]] QList<QAudioDevice> audioOutputs() const;

    /// @return All available audio input (microphone) devices.
    [[nodiscard]] QList<QAudioDevice> audioInputs() const;

    /// @return All available camera devices.
    [[nodiscard]] QList<QCameraDevice> cameras() const;

    /// @return All available screens (monitors) that can be captured.
    [[nodiscard]] QList<QScreen*> screens() const;
};

}  // namespace devicehub
