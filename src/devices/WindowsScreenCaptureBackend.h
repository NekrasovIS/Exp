#pragma once

#include "devices/IScreenCaptureBackend.h"

#include <memory>

namespace devicehub {

/**
 * @brief Реализация IScreenCaptureBackend поверх Windows.Graphics.Capture
 *        (WinRT), напрямую через Windows API, а не через Qt6
 *        QScreenCapture.
 *
 * Причина обхода Qt: в этой сборке Qt6 Multimedia (vcpkg) для Windows не
 * зарегистрирован backend захвата экрана — QScreenCapture::start()
 * возвращает NotSupportedError на любом экране (issue #154, диагностика
 * в комментарии к issue: камера при этом через тот же
 * QMediaCaptureSession работает нормально, так что это не общая
 * проблема мультимедиа-пайплайна, а именно отсутствующий backend
 * захвата экрана в этой сборке).
 *
 * Захватывает выбранный монитор через Direct3D11CaptureFramePool в
 * свободном потоке (CreateFreeThreaded — не требует message loop на
 * стороне вызывающего потока). Каждый кадр приходит на потоке из пула
 * ОС, поэтому перед испусканием frameAvailable() перемаршаллизируется в
 * GUI-поток через QMetaObject::invokeMethod(..., Qt::QueuedConnection) —
 * тот же паттерн, что уже используется в CallManager для кадров/событий
 * WebRTC (см. его doc-комментарий).
 *
 * Все типы WinRT/D3D11 спрятаны за pimpl — заголовок не тянет тяжёлые
 * системные заголовки в каждый файл, который просто держит указатель на
 * IScreenCaptureBackend.
 */
class WindowsScreenCaptureBackend : public IScreenCaptureBackend {
    Q_OBJECT

public:
    explicit WindowsScreenCaptureBackend(QObject* parent = nullptr);
    ~WindowsScreenCaptureBackend() override;

    void setScreen(QScreen* screen) override;
    void start() override;
    void stop() override;
    [[nodiscard]] bool isActive() const override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace devicehub
