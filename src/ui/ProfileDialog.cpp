#include "ui/ProfileDialog.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include "ui/Theme.h"
#include "user/UserProfileClient.h"

namespace devicehub {

ProfileDialog::ProfileDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Edit Profile"));
    setObjectName(QStringLiteral("profileDialog"));

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(ui_theme::kSpacingSm);

    displayNameEdit_ = new QLineEdit(this);
    displayNameEdit_->setObjectName(QStringLiteral("profileDisplayNameEdit"));
    displayNameEdit_->setPlaceholderText(tr("Display name"));

    avatarUrlEdit_ = new QLineEdit(this);
    avatarUrlEdit_->setObjectName(QStringLiteral("profileAvatarUrlEdit"));
    avatarUrlEdit_->setPlaceholderText(tr("Avatar URL"));

    emailEdit_ = new QLineEdit(this);
    emailEdit_->setObjectName(QStringLiteral("profileEmailEdit"));
    emailEdit_->setPlaceholderText(tr("Email (needed for one-time-code sign-in)"));

    telegramChatIdEdit_ = new QLineEdit(this);
    telegramChatIdEdit_->setObjectName(QStringLiteral("profileTelegramChatIdEdit"));
    telegramChatIdEdit_->setPlaceholderText(tr("Telegram chat ID (alternative one-time-code delivery)"));

    saveButton_ = new QPushButton(tr("Save"), this);
    saveButton_->setObjectName(QStringLiteral("profileSaveButton"));
    saveButton_->setProperty("accent", true);
    connect(saveButton_, &QPushButton::clicked, this, [this]() {
        emit saveRequested(ProfileEdits{.displayName = displayNameEdit_->text(),
                                         .avatarUrl = avatarUrlEdit_->text(),
                                         .email = emailEdit_->text(),
                                         .telegramChatId = telegramChatIdEdit_->text()});
    });

    // Issue #416: раньше единственным способом закрыть диалог без
    // сохранения было системное окно закрытия (или Escape, о котором
    // пользователь мог не догадаться) — теперь есть видимая кнопка
    // Cancel, тот же QDialogButtonBox, что уже используют другие
    // диалоги приложения (например, ChannelsPanel::showAddDialog()).
    // saveButton_ добавлен как ActionRole, а не Ok — клик по нему не
    // должен закрывать диалог (сохранение асинхронное, MainWindow сам
    // решает, когда диалог закрыть), поведение стандартной роли Ok
    // здесь не подходит.
    auto* buttonBox = new QDialogButtonBox(this);
    buttonBox->addButton(saveButton_, QDialogButtonBox::ActionRole);
    cancelButton_ = buttonBox->addButton(QDialogButtonBox::Cancel);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    statusLabel_ = new QLabel(this);
    statusLabel_->setObjectName(QStringLiteral("profileStatusLabel"));
    statusLabel_->setWordWrap(true);

    layout->addWidget(displayNameEdit_);
    layout->addWidget(avatarUrlEdit_);
    layout->addWidget(emailEdit_);
    layout->addWidget(telegramChatIdEdit_);
    layout->addWidget(buttonBox);
    layout->addWidget(statusLabel_);
}

void ProfileDialog::setProfile(const UserProfile& profile) {
    if (!displayNameEdit_->hasFocus()) {
        displayNameEdit_->setText(profile.displayName);
    }
    if (!avatarUrlEdit_->hasFocus()) {
        avatarUrlEdit_->setText(profile.avatarUrl);
    }
    if (!emailEdit_->hasFocus()) {
        emailEdit_->setText(profile.email);
    }
    if (!telegramChatIdEdit_->hasFocus()) {
        telegramChatIdEdit_->setText(profile.telegramChatId);
    }
}

}  // namespace devicehub
