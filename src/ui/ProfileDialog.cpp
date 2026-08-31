#include "ui/ProfileDialog.h"

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

    saveButton_ = new QPushButton(tr("Save"), this);
    saveButton_->setObjectName(QStringLiteral("profileSaveButton"));
    saveButton_->setProperty("accent", true);
    connect(saveButton_, &QPushButton::clicked, this,
            [this]() { emit saveRequested(displayNameEdit_->text(), avatarUrlEdit_->text()); });

    statusLabel_ = new QLabel(this);
    statusLabel_->setObjectName(QStringLiteral("profileStatusLabel"));
    statusLabel_->setWordWrap(true);

    layout->addWidget(displayNameEdit_);
    layout->addWidget(avatarUrlEdit_);
    layout->addWidget(saveButton_);
    layout->addWidget(statusLabel_);
}

void ProfileDialog::setProfile(const UserProfile& profile) {
    if (!displayNameEdit_->hasFocus()) {
        displayNameEdit_->setText(profile.displayName);
    }
    if (!avatarUrlEdit_->hasFocus()) {
        avatarUrlEdit_->setText(profile.avatarUrl);
    }
}

}  // namespace devicehub
