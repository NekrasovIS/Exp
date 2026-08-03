#include "devices/CameraDevice.h"

#include <QCamera>

namespace devicehub {

CameraDevice::CameraDevice(QObject* parent) : QObject(parent) {}

CameraDevice::~CameraDevice() = default;

void CameraDevice::setDevice(const QCameraDevice& device) {
    const bool wasActive = isActive();

    camera_ = std::make_unique<QCamera>(device);
    captureSession_.setCamera(camera_.get());

    connect(camera_.get(), &QCamera::errorOccurred, this,
            [this](QCamera::Error /*error*/, const QString& errorString) { emit errorOccurred(errorString); });

    if (wasActive) {
        start();
    }
}

void CameraDevice::start() {
    if (camera_) {
        camera_->start();
    }
}

void CameraDevice::stop() {
    if (camera_) {
        camera_->stop();
    }
}

bool CameraDevice::isActive() const {
    return camera_ && camera_->isActive();
}

QMediaCaptureSession& CameraDevice::captureSession() {
    return captureSession_;
}

}  // namespace devicehub
