#include "ui/DesktopNotifier.h"

#include <QSystemTrayIcon>
#include <QWidget>

#include "ui/IconFactory.h"
#include "ui/NotificationPolicy.h"

namespace devicehub {

namespace {
constexpr int kNotificationTimeoutMs = 5000;
}  // namespace

DesktopNotifier::DesktopNotifier(QWidget* mainWindow, QObject* parent) : QObject(parent), mainWindow_(mainWindow) {
    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        return;
    }
    trayIcon_ = new QSystemTrayIcon(this);
    trayIcon_->setIcon(ui_icons::communityAvatarIcon(QStringLiteral("D")));
    trayIcon_->setToolTip(QStringLiteral("DeviceHub"));
    connect(trayIcon_, &QSystemTrayIcon::messageClicked, this, [this]() {
        mainWindow_->activateWindow();
        mainWindow_->raise();
    });
    trayIcon_->show();
}

void DesktopNotifier::notifyMessage(const QString& author, const QString& body, const QString& currentUserLogin) {
    if (trayIcon_ == nullptr) {
        return;
    }
    if (!notification_policy::shouldNotify(mainWindow_->isActiveWindow(), author, currentUserLogin)) {
        return;
    }
    trayIcon_->showMessage(author, body, QSystemTrayIcon::Information, kNotificationTimeoutMs);
}

}  // namespace devicehub
