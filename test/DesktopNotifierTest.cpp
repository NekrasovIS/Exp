#include "ui/DesktopNotifier.h"

#include <gtest/gtest.h>

#include <QWidget>

namespace devicehub {
namespace {

// DesktopNotifier wraps a real QSystemTrayIcon and is deliberately a
// no-op when no system tray is available (see its class doc comment) —
// the actual notify-or-not decision lives in notification_policy,
// already covered by NotificationPolicyTest without needing a real tray.
// These are just smoke tests confirming construction and notifyMessage()
// are safe to call in either environment (tray available or not, own
// message or someone else's) rather than re-testing that decision logic.

TEST(DesktopNotifierTest, ConstructingDoesNotCrashRegardlessOfTrayAvailability) {
    QWidget mainWindow;

    DesktopNotifier notifier(&mainWindow);
}

TEST(DesktopNotifierTest, NotifyMessageForOwnMessageIsSafeToCall) {
    QWidget mainWindow;
    DesktopNotifier notifier(&mainWindow);

    notifier.notifyMessage(QStringLiteral("alice"), QStringLiteral("hi"), QStringLiteral("alice"));
}

TEST(DesktopNotifierTest, NotifyMessageForSomeoneElsesMessageIsSafeToCall) {
    QWidget mainWindow;
    DesktopNotifier notifier(&mainWindow);

    notifier.notifyMessage(QStringLiteral("bob"), QStringLiteral("hi"), QStringLiteral("alice"));
}

}  // namespace
}  // namespace devicehub
