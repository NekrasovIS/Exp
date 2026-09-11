#include "ui/PinnedMessagesDialog.h"

#include <gtest/gtest.h>

#include <QListWidget>
#include <QSignalSpy>

namespace devicehub {
namespace {

TEST(PinnedMessagesDialogTest, SetPinnedMessagesPopulatesListWithAuthorBodyAndPinnedBy) {
    PinnedMessagesDialog dialog;

    dialog.setPinnedMessages({PinnedMessageInfo{.id = 7,
                                                 .author = QStringLiteral("alice"),
                                                 .body = QStringLiteral("hi there"),
                                                 .sentAt = QStringLiteral("2026-08-05 09:00:00"),
                                                 .pinnedBy = QStringLiteral("bob"),
                                                 .pinnedAt = QStringLiteral("2026-08-05 10:00:00")}});

    ASSERT_EQ(dialog.pinnedList()->count(), 1);
    const QString text = dialog.pinnedList()->item(0)->text();
    EXPECT_TRUE(text.contains(QStringLiteral("alice")));
    EXPECT_TRUE(text.contains(QStringLiteral("hi there")));
    EXPECT_TRUE(text.contains(QStringLiteral("bob")));
    EXPECT_EQ(dialog.pinnedList()->item(0)->data(Qt::UserRole).toLongLong(), 7);
}

TEST(PinnedMessagesDialogTest, SetPinnedMessagesReplacesPreviousContents) {
    PinnedMessagesDialog dialog;
    dialog.setPinnedMessages({PinnedMessageInfo{.id = 1, .author = "a", .body = "one", .pinnedBy = "x"}});

    dialog.setPinnedMessages({PinnedMessageInfo{.id = 2, .author = "b", .body = "two", .pinnedBy = "x"},
                               PinnedMessageInfo{.id = 3, .author = "c", .body = "three", .pinnedBy = "x"}});

    EXPECT_EQ(dialog.pinnedList()->count(), 2);
}

TEST(PinnedMessagesDialogTest, SetPinnedMessagesWithEmptyListClearsIt) {
    PinnedMessagesDialog dialog;
    dialog.setPinnedMessages({PinnedMessageInfo{.id = 1, .author = "a", .body = "one", .pinnedBy = "x"}});

    dialog.setPinnedMessages({});

    EXPECT_EQ(dialog.pinnedList()->count(), 0);
}

TEST(PinnedMessagesDialogTest, ActivatingAnItemEmitsMessageActivatedWithMessageId) {
    PinnedMessagesDialog dialog;
    dialog.setPinnedMessages({PinnedMessageInfo{.id = 42, .author = "a", .body = "found me", .pinnedBy = "x"}});

    QSignalSpy spy(&dialog, &PinnedMessagesDialog::messageActivated);
    emit dialog.pinnedList()->itemActivated(dialog.pinnedList()->item(0));

    ASSERT_EQ(spy.count(), 1);
    EXPECT_EQ(spy.at(0).at(0).toLongLong(), 42);
}

}  // namespace
}  // namespace devicehub
