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
}

TEST(ProfileDialogTest, SetProfileFillsTheFields) {
    ProfileDialog dialog;

    dialog.setProfile(UserProfile{.login = QStringLiteral("alice"),
                                   .displayName = QStringLiteral("Alice"),
                                   .avatarUrl = QStringLiteral("https://example.test/alice.png")});

    EXPECT_EQ(dialog.displayNameEdit()->text(), QStringLiteral("Alice"));
    EXPECT_EQ(dialog.avatarUrlEdit()->text(), QStringLiteral("https://example.test/alice.png"));
}

TEST(ProfileDialogTest, ClickingSaveEmitsSaveRequestedWithCurrentFieldText) {
    ProfileDialog dialog;
    dialog.displayNameEdit()->setText(QStringLiteral("Bob"));
    dialog.avatarUrlEdit()->setText(QStringLiteral("https://example.test/bob.png"));

    QString emittedDisplayName;
    QString emittedAvatarUrl;
    int emitCount = 0;
    QObject::connect(&dialog, &ProfileDialog::saveRequested, [&](const QString& displayName, const QString& avatarUrl) {
        emittedDisplayName = displayName;
        emittedAvatarUrl = avatarUrl;
        ++emitCount;
    });

    emit dialog.saveButton()->clicked();

    EXPECT_EQ(emitCount, 1);
    EXPECT_EQ(emittedDisplayName, QStringLiteral("Bob"));
    EXPECT_EQ(emittedAvatarUrl, QStringLiteral("https://example.test/bob.png"));
}

}  // namespace
}  // namespace devicehub
