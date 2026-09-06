#include "devices/ScreenCaptureDevice.h"

#include "devices/IScreenCaptureBackend.h"

#if defined(Q_OS_WIN)
#include "devices/WindowsScreenCaptureBackend.h"
#endif

namespace devicehub {

ScreenCaptureDevice::ScreenCaptureDevice(QObject* parent) : QObject(parent) {
#if defined(Q_OS_WIN)
    backend_ = std::make_unique<WindowsScreenCaptureBackend>(this);
#endif
    if (backend_) {
        connect(backend_.get(), &IScreenCaptureBackend::frameAvailable, this, &ScreenCaptureDevice::frameAvailable);
        connect(backend_.get(), &IScreenCaptureBackend::errorOccurred, this, &ScreenCaptureDevice::errorOccurred);
    }
}

ScreenCaptureDevice::~ScreenCaptureDevice() = default;

void ScreenCaptureDevice::setScreen(QScreen* screen) {
    if (backend_) {
        backend_->setScreen(screen);
    }
}

void ScreenCaptureDevice::start() {
    if (backend_) {
        backend_->start();
    } else {
        emit errorOccurred(tr("Screen capture is not implemented on this platform."));
    }
}

void ScreenCaptureDevice::stop() {
    if (backend_) {
        backend_->stop();
    }
}

bool ScreenCaptureDevice::isActive() const {
    return backend_ && backend_->isActive();
}

}  // namespace devicehub
