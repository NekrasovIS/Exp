// Issue #311 — web analog of DeviceHub's DesktopNotifier/NotificationPolicy
// (src/ui/DesktopNotifier.cpp, src/ui/NotificationPolicy.cpp): a native
// notification for a new message in the open channel, while this tab
// isn't the one the user is looking at. shouldNotify() is the same pure
// decision function split out there for the same reason — testable
// without touching the real Notification API.

/** Mirrors notification_policy::shouldNotify() — @p hidden is this
 * tab's `document.hidden` (the web equivalent of DeviceHub's "main
 * window isn't the active window"). */
export function shouldNotify(hidden: boolean, messageAuthor: string, currentLogin: string | null): boolean {
  if (!hidden) {
    return false;
  }
  if (currentLogin !== null && messageAuthor === currentLogin) {
    return false;
  }
  return true;
}

/** No-op if the Notification API isn't available (older browsers,
 * jsdom in tests) or permission hasn't been granted — mirrors
 * DesktopNotifier's own no-op when QSystemTrayIcon::isSystemTrayAvailable()
 * is false, rather than throwing. Call once per app mount (HomePage) —
 * safe to call repeatedly, browsers only prompt once per origin. */
export function requestNotificationPermission(): void {
  if (typeof Notification === "undefined" || Notification.permission !== "default") {
    return;
  }
  void Notification.requestPermission();
}

/** Shows the actual notification — no-op unless the API is available
 * and permission was already granted. */
export function showMessageNotification(author: string, body: string): void {
  if (typeof Notification === "undefined" || Notification.permission !== "granted") {
    return;
  }
  new Notification(author, { body });
}
