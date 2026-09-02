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
 * @brief Владеет QScreenCapture и его сессией захвата для выбранного
 *        экрана.
 *
 * Отражает CameraDevice: QMediaCaptureSession поддерживает только один
 * video sink одновременно, поэтому этот класс сам владеет этим
 * единственным sink'ом и переиспускает каждый кадр через
 * frameAvailable(), а не открывает сессию наружу для прямого
 * подключения виджета — потребитель, желающий его отобразить (превью в
 * настройках, демонстрация экрана в звонках — issue #112), вместо этого
 * проталкивает кадры в свой собственный QVideoSink/цель. Это позволяет
 * и локальному виджету превью, и исходящему видео звонка делить один
 * захват экрана — тот же паттерн «один владелец одновременно», что уже
 * используется для CameraDevice/AudioInputDevice/AudioOutputDevice (см.
 * их doc-комментарии).
 */
class ScreenCaptureDevice : public QObject {
    Q_OBJECT

public:
    explicit ScreenCaptureDevice(QObject* parent = nullptr);
    ~ScreenCaptureDevice() override;

    /// Выбирает @p screen и готовит для него сессию захвата.
    void setScreen(QScreen* screen);

    /// Начинает захват выбранного экрана.
    void start();

    /// Останавливает захват.
    void stop();

    /// @return True, пока экран активно захватывается.
    [[nodiscard]] bool isActive() const;

    /// @return Сессия захвата — для чего угодно, кроме подключения
    /// video sink/вывода (этот класс уже владеет тем единственным
    /// слотом, что допускает QMediaCaptureSession); например, оставлено
    /// для будущей настройки на уровне сессии захвата.
    [[nodiscard]] QMediaCaptureSession& captureSession();

signals:
    /// Испускается для каждого захваченного кадра, независимо от того,
    /// чем занят захват экрана.
    void frameAvailable(const QVideoFrame& frame);

    /// Испускается, когда захват экрана сообщает об ошибке.
    void errorOccurred(const QString& message);

private:
    std::unique_ptr<QScreenCapture> screenCapture_;
    QMediaCaptureSession captureSession_;
    QVideoSink videoSink_;
};

}  // namespace devicehub
