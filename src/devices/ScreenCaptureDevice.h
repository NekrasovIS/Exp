#pragma once

#include <QMediaCaptureSession>
#include <QObject>
#include <memory>

class QScreen;
class QScreenCapture;

namespace devicehub {

/**
 * @brief Owns a QScreenCapture and its capture session for a chosen screen.
 *
 * Mirrors CameraDevice: exposes the underlying QMediaCaptureSession so a
 * QVideoWidget (owned by the UI layer) can be attached as the preview
 * sink, keeping this class unaware of any widget/rendering concerns.
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

    /// @return The capture session, for attaching a QVideoWidget/QVideoSink.
    [[nodiscard]] QMediaCaptureSession& captureSession();

signals:
    /// Emitted when screen capture reports an error.
    void errorOccurred(const QString& message);

private:
    std::unique_ptr<QScreenCapture> screenCapture_;
    QMediaCaptureSession captureSession_;
};

}  // namespace devicehub
