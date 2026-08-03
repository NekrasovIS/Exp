#include "devices/DeviceEnumerator.h"

#include <QMediaDevices>

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

}  // namespace devicehub
