#include "ui/ProfileDialog.h"

#include <gtest/gtest.h>

#include <QDialog>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalSpy>

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

// Issue #384/#442 — выбор файла аватара. Диалог сам не открывает
// QFileDialog (см. doc-комментарий ProfileDialog::chooseAvatarFileRequested()) —
// клик по кнопке только эмитит сигнал, MainWindow делает всё остальное,
// поэтому тест здесь не блокируется на реальном системном диалоге.

TEST(ProfileDialogTest, ClickingChooseAvatarFileButtonEmitsChooseAvatarFileRequested) {
    ProfileDialog dialog;
    QSignalSpy spy(&dialog, &ProfileDialog::chooseAvatarFileRequested);

    dialog.chooseAvatarFileButton()->click();

    EXPECT_EQ(spy.count(), 1);
}

TEST(ProfileDialogTest, SetAvatarImageReplacesTheLetterPreviewWithARealPhoto) {
    ProfileDialog dialog;
    const QImage letterPixmap = dialog.avatarPreviewLabel()->pixmap().toImage();

    QImage realPhoto(64, 64, QImage::Format_ARGB32);
    realPhoto.fill(Qt::red);
    dialog.setAvatarImage(realPhoto);

    EXPECT_FALSE(dialog.avatarPreviewLabel()->pixmap().isNull());
    EXPECT_NE(dialog.avatarPreviewLabel()->pixmap().toImage(), letterPixmap);
}

// Issue #416: Cancel — новая кнопка, добавленная через QDialogButtonBox
// (раньше диалог можно было закрыть только системным окном/Escape).

TEST(ProfileDialogTest, ClickingCancelClosesWithoutEmittingSaveRequested) {
    ProfileDialog dialog;
    dialog.displayNameEdit()->setText(QStringLiteral("Bob"));

    int emitCount = 0;
    QObject::connect(&dialog, &ProfileDialog::saveRequested, [&](const ProfileEdits&) { ++emitCount; });

    dialog.cancelButton()->click();

    EXPECT_EQ(emitCount, 0);
    EXPECT_EQ(dialog.result(), QDialog::Rejected);
}

}  // namespace
}  // namespace devicehub
