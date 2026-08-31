#pragma once

#include <QObject>
#include <QString>

class QSystemTrayIcon;
class QWidget;

namespace devicehub {

/**
 * @brief Shows a native OS notification for a new chat message when
 *        @p mainWindow (passed in at construction) isn't the active
 *        window, so the user doesn't have to keep DeviceHub focused to
 *        notice new messages (issue #92).
 *
 * Wraps QSystemTrayIcon — a no-op (no tray icon created, notifyMessage()
 * a no-op) if QSystemTrayIcon::isSystemTrayAvailable() is false, e.g.
 * headless CI. The actual show-or-not decision is
 * notification_policy::shouldNotify(), kept separate so it's testable
 * without a real system tray.
 */
class DesktopNotifier : public QObject {
    Q_OBJECT

public:
    explicit DesktopNotifier(QWidget* mainWindow, QObject* parent = nullptr);

    /// Shows a notification for @p author's @p body if
    /// notification_policy::shouldNotify() says to, given the main
    /// window's current isActiveWindow() and @p currentUserLogin.
    void notifyMessage(const QString& author, const QString& body, const QString& currentUserLogin);

private:
    QWidget* mainWindow_;
    QSystemTrayIcon* trayIcon_ = nullptr;
};

}  // namespace devicehub
