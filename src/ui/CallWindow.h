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

    /// Обновляет подпись кнопки видео и видимость локального превью
    /// камеры — независимо от setScreenShareEnabled() (issue #185, оба
    /// можно включить одновременно).
    void setVideoEnabled(bool enabled);

    /// То же самое, что setVideoEnabled(), для отдельного превью
    /// демонстрации экрана (issue #185 — свой QVideoWidget, а не общий с
    /// камерой, раз оба теперь могут быть активны одновременно).
    void setScreenShareEnabled(bool enabled);

    /// Список участников звонка, кроме нас самих — пустой список
    /// скрывает подпись целиком.
    void setCallParticipants(const QStringList& participants);

    /// Создаёт (при первом кадре такого рода от @p peerLogin) или
    /// обновляет плитку удалённого видео этого участника. @p isScreenShare
    /// различает камеру и демонстрацию экрана одного и того же участника
    /// (issue #185) — у каждого рода своя плитка, оба могут быть видны
    /// одновременно.
    void showRemoteVideoFrame(const QString& peerLogin, const QImage& frame, bool isScreenShare);

    /// Убирает плитку удалённого видео @p peerLogin рода @p isScreenShare,
    /// если она была — например, когда участник перестал слать её или
    /// вышел из звонка.
    void removeRemoteVideo(const QString& peerLogin, bool isScreenShare);

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
    /// Отдельное превью демонстрации экрана (issue #185) — не то же
    /// самое, что localVideoWidget() (камера): оба могут быть видны
    /// одновременно.
    [[nodiscard]] QVideoWidget* localScreenShareVideoWidget() const { return localScreenShareVideoWidget_; }

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
    /// Пересчитывает видимость всей videoStrip_ — видна, пока показывать
    /// есть что: локальная камера, локальная демонстрация экрана или
    /// хотя бы одна удалённая плитка (issue #185 — все три источника
    /// теперь независимы друг от друга, а не два взаимоисключающих).
    void updateVideoStripVisibility();

    QLabel* callParticipantsLabel_ = nullptr;
    QPushButton* muteToggleButton_ = nullptr;
    QPushButton* videoToggleButton_ = nullptr;
    QPushButton* screenShareToggleButton_ = nullptr;
    QPushButton* leaveCallButton_ = nullptr;
    QWidget* videoStrip_ = nullptr;
    QHBoxLayout* videoStripLayout_ = nullptr;
    QVideoWidget* localVideoWidget_ = nullptr;
    QVideoWidget* localScreenShareVideoWidget_ = nullptr;
    /// Ключ — "<login>#camera" или "<login>#screen" (issue #185), чтобы
    /// камера и демонстрация экрана одного участника не делили одну
    /// плитку.
    QHash<QString, QLabel*> remoteVideoTiles_;
    bool videoActive_ = false;
    bool screenShareActive_ = false;
};

}  // namespace devicehub
