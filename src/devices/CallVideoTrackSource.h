#pragma once

#include <media/base/adapted_video_track_source.h>

#include <QVideoFrame>

#include <atomic>
#include <optional>

namespace devicehub {

/**
 * @brief Соединяет кадры с камеры/захвата экрана Qt с видео-конвейером
 *        libwebrtc — видео-аналог CallAudioDeviceModule (issue #64):
 *        у Qt Multimedia нет встроенного способа проталкивать кадры в
 *        webrtc::VideoTrackSourceInterface, поэтому этот класс
 *        существует как такой мост.
 *
 * pushFrame() конвертирует каждый QVideoFrame в I420 через QImage как
 * промежуточный шаг (QVideoFrame::toImage(), затем
 * libyuv::ARGBToI420()), а не разбирает вручную каждый возможный
 * нативный формат пикселей камеры напрямую — проще и надёжнее ценой
 * дополнительного прохода конвертации, что соответствует объёму «первый
 * проход, ещё не настроено на пиковую эффективность», как и у
 * CallAudioDeviceModule. Передаёт результат в
 * AdaptedVideoTrackSource::OnFrame(), которая — через AdaptFrame() —
 * также занимается адаптацией разрешения для каждого sink'а (например,
 * приёмник просит кадр поменьше под давлением пропускной способности) и
 * всей регистрацией/управлением sink'ами.
 */
class CallVideoTrackSource : public webrtc::AdaptedVideoTrackSource {
public:
    /// @p isScreencast должен быть true для источника демонстрации
    /// экрана — см. контракт is_screencast() в
    /// webrtc::VideoTrackSourceInterface (позволяет приёмникам применять
    /// значения по умолчанию, подходящие для screencast).
    explicit CallVideoTrackSource(bool isScreencast = false);

    /// Конвертирует и проталкивает захваченный кадр в конвейер.
    /// Безопасно вызывать из любого потока (соответствует собственному
    /// контракту OnFrame()/AdaptFrame() у AdaptedVideoTrackSource);
    /// ничего не делает, если кадр невалиден или нет заинтересованных
    /// sink'ов.
    void pushFrame(const QVideoFrame& frame);

    /// Переключает подсказку screencast на уже созданном источнике —
    /// на случай, если роль источника (камера/демонстрация экрана)
    /// когда-либо понадобится менять после конструирования, а не
    /// фиксировать один раз через конструктор. CallManager (issue #112,
    /// #185) этим не пользуется — у него по одному отдельному источнику
    /// на камеру и на демонстрацию экрана, каждый с фиксированной ролью
    /// с самого создания. Потокобезопасно, как и остальная публичная
    /// поверхность этого класса — is_screencast() может опрашиваться с
    /// потока WebRTC, пока это вызывается с GUI-потока.
    void setIsScreencast(bool isScreencast);

    bool is_screencast() const override;
    std::optional<bool> needs_denoising() const override;
    SourceState state() const override;
    bool remote() const override;

private:
    std::atomic<bool> isScreencast_;
};

}  // namespace devicehub
