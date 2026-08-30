#include "ui/NotificationPolicy.h"

#include <gtest/gtest.h>

namespace devicehub {
namespace {

TEST(NotificationPolicyTest, DoesNotNotifyWhileWindowIsActive) {
    EXPECT_FALSE(notification_policy::shouldNotify(/*windowActive=*/true, QStringLiteral("alice"),
                                                     QStringLiteral("bob")));
}

TEST(NotificationPolicyTest, DoesNotNotifyForOwnMessage) {
    EXPECT_FALSE(notification_policy::shouldNotify(/*windowActive=*/false, QStringLiteral("bob"),
                                                     QStringLiteral("bob")));
}

TEST(NotificationPolicyTest, NotifiesForSomeoneElsesMessageWhileWindowInactive) {
    EXPECT_TRUE(notification_policy::shouldNotify(/*windowActive=*/false, QStringLiteral("alice"),
                                                    QStringLiteral("bob")));
}

TEST(NotificationPolicyTest, NotifiesWhenNotSignedInYet) {
    EXPECT_TRUE(
        notification_policy::shouldNotify(/*windowActive=*/false, QStringLiteral("alice"), QStringLiteral("")));
}

}  // namespace
}  // namespace devicehub
