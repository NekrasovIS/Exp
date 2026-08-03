#pragma once

#include <QCameraDevice>
#include <QMediaCaptureSession>
#include <QObject>
#include <memory>

class QCamera;
class QVideoWidget;

namespace devicehub {

/**
 * @brief Owns a QCamera and its capture session for a chosen camera device.
 *
 * Exposes the underlying QMediaCaptureSession so a QVideoWidget (owned by
 * the UI layer) can be attached as the preview sink, keeping this class
 * unaware of any widget/rendering concerns.
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

    /// @return The capture session, for attaching a QVideoWidget/QVideoSink.
    [[nodiscard]] QMediaCaptureSession& captureSession();

signals:
    /// Emitted when the camera reports an error.
    void errorOccurred(const QString& message);

private:
    std::unique_ptr<QCamera> camera_;
    QMediaCaptureSession captureSession_;
};

}  // namespace devicehub
