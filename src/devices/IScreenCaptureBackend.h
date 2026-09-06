#pragma once

#include <QObject>
#include <QVideoFrame>

class QScreen;

namespace devicehub {

/**
 * @brief Платформенный источник кадров для ScreenCaptureDevice.
 *
 * ScreenCaptureDevice остаётся кросс-платформенным фасадом (см. его
 * doc-комментарий) — весь код, обращающийся напрямую к API конкретной
 * ОС для захвата экрана, живёт за этим интерфейсом в реализациях вроде
 * WindowsScreenCaptureBackend, а не в виде #ifdef внутри бизнес-логики
 * (см. раздел «Кроссплатформенность» в CLAUDE.md).
 */
class IScreenCaptureBackend : public QObject {
    Q_OBJECT

public:
    explicit IScreenCaptureBackend(QObject* parent = nullptr) : QObject(parent) {}
    ~IScreenCaptureBackend() override = default;

    /// Выбирает экран для захвата — реализация должна быть готова к
    /// повторному вызову (смена экрана на лету).
    virtual void setScreen(QScreen* screen) = 0;

    /// Начинает захват выбранного экрана.
    virtual void start() = 0;

    /// Останавливает захват.
    virtual void stop() = 0;

    /// @return True, пока экран активно захватывается.
    [[nodiscard]] virtual bool isActive() const = 0;

signals:
    /// Испускается для каждого захваченного кадра — гарантированно из
    /// GUI-потока, независимо от того, на каком потоке ОС реально
    /// доставляет кадры реализации.
    void frameAvailable(const QVideoFrame& frame);

    /// Испускается, когда захват экрана сообщает об ошибке.
    void errorOccurred(const QString& message);
};

}  // namespace devicehub
