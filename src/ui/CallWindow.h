#pragma once

#include <QHash>
#include <QStringList>
#include <QWidget>

class QHBoxLayout;
class QImage;
class QLabel;
class QPushButton;
class QVideoWidget;

namespace devicehub {

/**
 * @brief Отдельное окно звонка (issue #185) — участники, элементы
 *        управления (mute/видео/демонстрация экрана/выход) и видео
 *        (локальное превью + по одной плитке на удалённого участника),
 *        вынесенные из ChatView, чтобы звонок не делил пространство с
 *        чатом.
 *
 * Чистое представление, как и ChatView — не знает про CallManager,
 * только сигнализирует запросы переключения и получает обратно готовое
 * состояние через сеттеры; MainWindow решает, какое действие выполнить
 * над CallManager, и вызывает сеттеры с результатом. Показывается/
 * скрывается MainWindow-ом по фактическому переходу в/из звонка — сама
 * не решает, когда её открыть.
 */
class CallWindow : public QWidget {
    Q_OBJECT

public:
    explicit CallWindow(QWidget* parent = nullptr);

    /// Обновляет подпись/состояние кнопки Mute.
    void setMuted(bool muted);

    /// Обновляет подпись кнопки видео и видимость локального превью —
    /// та же логика, что раньше жила в ChatView::setVideoEnabled().
    void setVideoEnabled(bool enabled);

    /// Обновляет подпись кнопки демонстрации экрана и видимость
    /// локального превью — та же логика, что раньше жила в
    /// ChatView::setScreenShareEnabled().
    void setScreenShareEnabled(bool enabled);

    /// Список участников звонка, кроме нас самих — пустой список
    /// скрывает подпись целиком.
    void setCallParticipants(const QStringList& participants);

    /// Создаёт (при первом кадре от @p peerLogin) или обновляет плитку
    /// удалённого видео этого участника.
    void showRemoteVideoFrame(const QString& peerLogin, const QImage& frame);

    /// Убирает плитку удалённого видео участника @p peerLogin, если она
    /// была — например, когда участник перестал слать видео или вышел
    /// из звонка.
    void removeRemoteVideo(const QString& peerLogin);

    /// Сбрасывает состояние окна к «звонка нет» — вызывается
    /// MainWindow-ом при выходе из звонка, перед скрытием окна: снимает
    /// все плитки удалённого видео и подпись участников, чтобы
    /// следующий звонок не унаследовал их от предыдущего.
    void resetForNewCall();

    [[nodiscard]] QPushButton* muteToggleButton() const { return muteToggleButton_; }
    [[nodiscard]] QPushButton* videoToggleButton() const { return videoToggleButton_; }
    [[nodiscard]] QPushButton* screenShareToggleButton() const { return screenShareToggleButton_; }
    [[nodiscard]] QPushButton* leaveCallButton() const { return leaveCallButton_; }
    [[nodiscard]] QLabel* callParticipantsLabel() const { return callParticipantsLabel_; }
    [[nodiscard]] QVideoWidget* localVideoWidget() const { return localVideoWidget_; }

signals:
    void muteToggleRequested();
    void videoToggleRequested();
    void screenShareToggleRequested();
    /// Клик по "Leave call" в самом окне звонка — MainWindow подключает
    /// это к тому же обработчику, что и ChatView::callToggleRequested()
    /// в состоянии "уже в звонке" (оба сводятся к одному и тому же
    /// «выйти из звонка»).
    void leaveCallRequested();

private:
    /// Пересчитывает видимость localVideoWidget_/окна плиток целиком —
    /// общая логика для setVideoEnabled()/setScreenShareEnabled(), как
    /// и раньше в ChatView (камера и демонстрация экрана в CallManager
    /// взаимоисключающие, но оба сеттера вызываются на каждое
    /// переключение — см. их вызовы в MainWindow).
    void updateLocalVideoVisibility();

    QLabel* callParticipantsLabel_ = nullptr;
    QPushButton* muteToggleButton_ = nullptr;
    QPushButton* videoToggleButton_ = nullptr;
    QPushButton* screenShareToggleButton_ = nullptr;
    QPushButton* leaveCallButton_ = nullptr;
    QWidget* videoStrip_ = nullptr;
    QHBoxLayout* videoStripLayout_ = nullptr;
    QVideoWidget* localVideoWidget_ = nullptr;
    QHash<QString, QLabel*> remoteVideoTiles_;
    bool videoActive_ = false;
    bool screenShareActive_ = false;
};

}  // namespace devicehub
