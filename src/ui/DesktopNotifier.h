#pragma once

#include <QObject>
#include <QString>

class QSystemTrayIcon;
class QWidget;

namespace devicehub {

/**
 * @brief Показывает нативное системное уведомление о новом сообщении
 *        чата, когда @p mainWindow (передаётся при конструировании) не
 *        является активным окном, чтобы пользователю не приходилось
 *        держать DeviceHub в фокусе, чтобы замечать новые сообщения
 *        (issue #92).
 *
 * Оборачивает QSystemTrayIcon — ничего не делает (значок в трее не
 * создаётся, notifyMessage() — no-op), если
 * QSystemTrayIcon::isSystemTrayAvailable() возвращает false, например
 * в headless CI. Фактическое решение показывать или нет —
 * notification_policy::shouldNotify(), вынесено отдельно, чтобы быть
 * тестируемым без реального системного трея.
 */
class DesktopNotifier : public QObject {
    Q_OBJECT

public:
    explicit DesktopNotifier(QWidget* mainWindow, QObject* parent = nullptr);

    /// Показывает уведомление с @p body от @p author, если
    /// notification_policy::shouldNotify() велит это сделать, исходя из
    /// текущего isActiveWindow() главного окна и @p currentUserLogin.
    void notifyMessage(const QString& author, const QString& body, const QString& currentUserLogin);

private:
    QWidget* mainWindow_;
    QSystemTrayIcon* trayIcon_ = nullptr;
};

}  // namespace devicehub
