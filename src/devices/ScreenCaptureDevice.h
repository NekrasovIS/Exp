#pragma once

#include <QMediaCaptureSession>
#include <QObject>
#include <QVideoFrame>
#include <QVideoSink>
#include <memory>

class QScreen;
class QScreenCapture;

namespace devicehub {

/**
 * @brief Owns a QScreenCapture and its capture session for a chosen screen.
 *
 * Mirrors CameraDevice: QMediaCaptureSession only supports one video sink
 * at a time, so this class owns that one sink itself and re-emits every
 * frame via frameAvailable() rather than exposing the session for a
 * widget to attach directly — a consumer that wants to display it (the
 * settings preview, screen share in calls — issue #112) pushes frames
 * into its own QVideoSink/target instead. This lets both a local preview
 * widget and a call's outgoing video share the one screen capture, the
 * same single-owner-at-a-time pattern already used for CameraDevice/
 * AudioInputDevice/AudioOutputDevice (see their doc comments).
 */
class ScreenCaptureDevice : public QObject {
    Q_OBJECT

public:
    explicit ScreenCaptureDevice(QObject* parent = nullptr);
    ~ScreenCaptureDevice() override;

    /// Selects @p screen and prepares the capture session for it.
    void setScreen(QScreen* screen);

    /// Starts capturing the selected screen.
    void start();

    /// Stops capturing.
    void stop();

    /// @return True while the screen is actively being captured.
    [[nodiscard]] bool isActive() const;

    /// @return The capture session — for anything other than attaching a
    /// video sink/output (this class already owns the one slot
    /// QMediaCaptureSession allows); e.g. reserved for future
    /// capture-session-level configuration.
    [[nodiscard]] QMediaCaptureSession& captureSession();

signals:
    /// Emitted for every captured frame, whatever the screen's active.
    void frameAvailable(const QVideoFrame& frame);

    /// Emitted when screen capture reports an error.
    void errorOccurred(const QString& message);

private:
    std::unique_ptr<QScreenCapture> screenCapture_;
    QMediaCaptureSession captureSession_;
    QVideoSink videoSink_;
};

}  // namespace devicehub
