#include "ui/NotificationPolicy.h"

#include <QString>

namespace devicehub::notification_policy {

bool shouldNotify(bool windowActive, const QString& messageAuthor, const QString& currentUserLogin) {
    if (windowActive) {
        return false;
    }
    if (!currentUserLogin.isEmpty() && messageAuthor == currentUserLogin) {
        return false;
    }
    return true;
}

}  // namespace devicehub::notification_policy
