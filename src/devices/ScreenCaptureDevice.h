#pragma once

#include <QObject>
#include <QVideoFrame>
#include <memory>

class QScreen;

namespace devicehub {

class IScreenCaptureBackend;

/**
 * @brief Кросс-платформенный фасад захвата экрана.
 *
 * Сам не обращается к API конкретной ОС — делегирует
 * IScreenCaptureBackend (WindowsScreenCaptureBackend на Windows; на
 * прочих платформах пока не реализовано, см. конструктор), выбранному
 * ровно в одной точке (конструкторе), а не разбросанными по бизнес-
 * логике #ifdef (см. «Кроссплатформенность» в CLAUDE.md).
 *
 * До issue #154 здесь использовался Qt6 QScreenCapture — заменён,
 * потому что в этой сборке Qt6 Multimedia (vcpkg) backend захвата
 * экрана для Windows не собран (QScreenCapture::start() возвращал
 * NotSupportedError на любом экране; см. диагностику в issue).
 *
 * Отражает CameraDevice: только один потребитель кадров одновременно
 * технически не требуется здесь (в отличие от QMediaCaptureSession),
 * но сохранён тот же паттерн одного владельца, переизлучающего кадры
 * через frameAvailable() всем интересующимся (превью в настройках,
 * демонстрация экрана в звонках — issue #112) — тот же принцип, что и
 * у CameraDevice/AudioInputDevice/AudioOutputDevice (см. их doc-
 * комментарии).
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

signals:
    /// Испускается для каждого захваченного кадра, независимо от того,
    /// чем занят захват экрана.
    void frameAvailable(const QVideoFrame& frame);

    /// Испускается, когда захват экрана сообщает об ошибке.
    void errorOccurred(const QString& message);

private:
    std::unique_ptr<IScreenCaptureBackend> backend_;
};

}  // namespace devicehub
