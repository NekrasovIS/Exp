#include "ui/ProfileDialog.h"

#include <gtest/gtest.h>

#include <QLineEdit>
#include <QPushButton>

#include "user/UserProfileClient.h"

namespace devicehub {
namespace {

TEST(ProfileDialogTest, FieldsStartEmpty) {
    ProfileDialog dialog;

    EXPECT_TRUE(dialog.displayNameEdit()->text().isEmpty());
    EXPECT_TRUE(dialog.avatarUrlEdit()->text().isEmpty());
    EXPECT_TRUE(dialog.emailEdit()->text().isEmpty());
    EXPECT_TRUE(dialog.telegramChatIdEdit()->text().isEmpty());
}

TEST(ProfileDialogTest, SetProfileFillsTheFields) {
    ProfileDialog dialog;

    dialog.setProfile(UserProfile{.login = QStringLiteral("alice"),
                                   .displayName = QStringLiteral("Alice"),
                                   .avatarUrl = QStringLiteral("https://example.test/alice.png"),
                                   .email = QStringLiteral("alice@example.test"),
                                   .telegramChatId = QStringLiteral("123456789")});

    EXPECT_EQ(dialog.displayNameEdit()->text(), QStringLiteral("Alice"));
    EXPECT_EQ(dialog.avatarUrlEdit()->text(), QStringLiteral("https://example.test/alice.png"));
    EXPECT_EQ(dialog.emailEdit()->text(), QStringLiteral("alice@example.test"));
    EXPECT_EQ(dialog.telegramChatIdEdit()->text(), QStringLiteral("123456789"));
}

TEST(ProfileDialogTest, ClickingSaveEmitsSaveRequestedWithCurrentFieldText) {
    ProfileDialog dialog;
    dialog.displayNameEdit()->setText(QStringLiteral("Bob"));
    dialog.avatarUrlEdit()->setText(QStringLiteral("https://example.test/bob.png"));
    dialog.emailEdit()->setText(QStringLiteral("bob@example.test"));
    dialog.telegramChatIdEdit()->setText(QStringLiteral("987654321"));

    // A direct lambda connection, not QSignalSpy — QSignalSpy boxes
    // every argument as QVariant, which would need ProfileEdits
    // registered as a Qt metatype (Q_DECLARE_METATYPE) just for this
    // one test; a plain connection needs no such registration.
    ProfileEdits emitted;
    int emitCount = 0;
    QObject::connect(&dialog, &ProfileDialog::saveRequested, [&](const ProfileEdits& edits) {
        emitted = edits;
        ++emitCount;
    });

    dialog.saveButton()->click();

    EXPECT_EQ(emitCount, 1);
    EXPECT_EQ(emitted.displayName, QStringLiteral("Bob"));
    EXPECT_EQ(emitted.avatarUrl, QStringLiteral("https://example.test/bob.png"));
    EXPECT_EQ(emitted.email, QStringLiteral("bob@example.test"));
    EXPECT_EQ(emitted.telegramChatId, QStringLiteral("987654321"));
}

}  // namespace
}  // namespace devicehub
