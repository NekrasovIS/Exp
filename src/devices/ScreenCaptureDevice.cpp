#include "devices/ScreenCaptureDevice.h"

#include <QScreenCapture>

namespace devicehub {

ScreenCaptureDevice::ScreenCaptureDevice(QObject* parent) : QObject(parent) {}

ScreenCaptureDevice::~ScreenCaptureDevice() = default;

void ScreenCaptureDevice::setScreen(QScreen* screen) {
    const bool wasActive = isActive();

    screenCapture_ = std::make_unique<QScreenCapture>();
    screenCapture_->setScreen(screen);
    captureSession_.setScreenCapture(screenCapture_.get());

    connect(screenCapture_.get(), &QScreenCapture::errorOccurred, this,
            [this](QScreenCapture::Error /*error*/, const QString& errorString) { emit errorOccurred(errorString); });

    if (wasActive) {
        start();
    }
}

void ScreenCaptureDevice::start() {
    if (screenCapture_) {
        screenCapture_->start();
    }
}

void ScreenCaptureDevice::stop() {
    if (screenCapture_) {
        screenCapture_->stop();
    }
}

bool ScreenCaptureDevice::isActive() const {
    return screenCapture_ && screenCapture_->isActive();
}

QMediaCaptureSession& ScreenCaptureDevice::captureSession() {
    return captureSession_;
}

}  // namespace devicehub
