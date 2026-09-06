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
 * @brief Владеет QCamera и его сессией захвата для выбранного
 *        устройства камеры.
 *
 * QMediaCaptureSession поддерживает только один video sink
 * одновременно, поэтому этот класс сам владеет этим единственным
 * sink'ом и переиспускает каждый кадр через frameAvailable(), а не
 * открывает сессию наружу для прямого подключения виджета —
 * потребитель, желающий его отобразить (превью в настройках,
 * CallVideoTrackSource для звонков из issue #72), вместо этого
 * проталкивает кадры в свой собственный QVideoSink/цель. Это позволяет
 * и локальному виджету превью, и исходящему видео звонка делить один
 * захват с камеры — тот же паттерн «один владелец одновременно», что
 * уже используется для AudioInputDevice/AudioOutputDevice (см. их
 * doc-комментарии).
 */
class CameraDevice : public QObject {
    Q_OBJECT

public:
    explicit CameraDevice(QObject* parent = nullptr);
    ~CameraDevice() override;

    /// Выбирает @p device и готовит для него сессию захвата.
    void setDevice(const QCameraDevice& device);

    /// Запускает поток с камеры.
    void start();

    /// Останавливает поток с камеры.
    void stop();

    /// @return True, пока камера активно стримит.
    [[nodiscard]] bool isActive() const;

    /// @return Сессия захвата — для чего угодно, кроме подключения
    /// video sink/вывода (этот класс уже владеет тем единственным
    /// слотом, что допускает QMediaCaptureSession); например, оставлено
    /// для будущей настройки на уровне сессии захвата.
    [[nodiscard]] QMediaCaptureSession& captureSession();

signals:
    /// Испускается для каждого захваченного кадра, независимо от того,
    /// чем занята камера.
    void frameAvailable(const QVideoFrame& frame);

    /// Испускается, когда камера сообщает об ошибке.
    void errorOccurred(const QString& message);

private:
    std::unique_ptr<QCamera> camera_;
    QMediaCaptureSession captureSession_;
    QVideoSink videoSink_;
};

}  // namespace devicehub
