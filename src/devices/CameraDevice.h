#pragma once

#include <QCameraDevice>
#include <QMediaCaptureSession>
#include <QObject>
#include <QVideoFrame>
#include <QVideoSink>
#include <memory>

class QCamera;
class QVideoWidget;

namespace devicehub {

/**
 * @brief Owns a QCamera and its capture session for a chosen camera device.
 *
 * QMediaCaptureSession only supports one video sink at a time, so this
 * class owns that one sink itself and re-emits every frame via
 * frameAvailable() rather than exposing the session for a widget to
 * attach directly — a consumer that wants to display it (the settings
 * preview, issue #72's CallVideoTrackSource for calls) pushes frames
 * into its own QVideoSink/target instead. This lets both a local
 * preview widget and a call's outgoing video share the one camera
 * capture, the same single-owner-at-a-time pattern already used for
 * AudioInputDevice/AudioOutputDevice (see their doc comments).
 */
class CameraDevice : public QObject {
    Q_OBJECT

public:
    explicit CameraDevice(QObject* parent = nullptr);
    ~CameraDevice() override;

    /// Selects @p device and prepares the capture session for it.
    void setDevice(const QCameraDevice& device);

    /// Starts the camera stream.
    void start();

    /// Stops the camera stream.
    void stop();

    /// @return True while the camera is actively streaming.
    [[nodiscard]] bool isActive() const;

    /// @return The capture session — for anything other than attaching
    /// a video sink/output (this class already owns the one slot
    /// QMediaCaptureSession allows); e.g. reserved for future
    /// capture-session-level configuration.
    [[nodiscard]] QMediaCaptureSession& captureSession();

signals:
    /// Emitted for every captured frame, whatever the camera's active.
    void frameAvailable(const QVideoFrame& frame);

    /// Emitted when the camera reports an error.
    void errorOccurred(const QString& message);

private:
    std::unique_ptr<QCamera> camera_;
    QMediaCaptureSession captureSession_;
    QVideoSink videoSink_;
};

}  // namespace devicehub
