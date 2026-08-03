#include "devices/DeviceEnumerator.h"

#include <QGuiApplication>
#include <QMediaDevices>
#include <QScreen>

namespace devicehub {

QList<QAudioDevice> DeviceEnumerator::audioOutputs() const {
    return QMediaDevices::audioOutputs();
}

QList<QAudioDevice> DeviceEnumerator::audioInputs() const {
    return QMediaDevices::audioInputs();
}

QList<QCameraDevice> DeviceEnumerator::cameras() const {
    return QMediaDevices::videoInputs();
}

QList<QScreen*> DeviceEnumerator::screens() const {
    return QGuiApplication::screens();
}

}  // namespace devicehub
