#include "ui/ModeratorsDialog.h"

#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

#include "ui/Theme.h"

namespace devicehub {

ModeratorsDialog::ModeratorsDialog(QWidget* parent) : QDialog(parent) {
    setObjectName(QStringLiteral("moderatorsDialog"));

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(ui_theme::kSpacingSm);

    moderatorsList_ = new QListWidget(this);
    moderatorsList_->setObjectName(QStringLiteral("moderatorsList"));

    loginEdit_ = new QLineEdit(this);
    loginEdit_->setObjectName(QStringLiteral("moderatorLoginEdit"));
    loginEdit_->setPlaceholderText(tr("Login to promote"));

    promoteButton_ = new QPushButton(tr("Promote"), this);
    promoteButton_->setObjectName(QStringLiteral("promoteModeratorButton"));
    promoteButton_->setProperty("accent", true);
    connect(promoteButton_, &QPushButton::clicked, this, [this]() {
        const QString login = loginEdit_->text().trimmed();
        if (!login.isEmpty()) {
            emit promoteRequested(communityId_, login);
            loginEdit_->clear();
        }
    });

    demoteButton_ = new QPushButton(tr("Demote Selected"), this);
    demoteButton_->setObjectName(QStringLiteral("demoteModeratorButton"));
    connect(demoteButton_, &QPushButton::clicked, this, [this]() {
        if (QListWidgetItem* item = moderatorsList_->currentItem(); item != nullptr) {
            emit demoteRequested(communityId_, item->text());
        }
    });

    statusLabel_ = new QLabel(this);
    statusLabel_->setObjectName(QStringLiteral("moderatorsStatusLabel"));
    statusLabel_->setWordWrap(true);

    layout->addWidget(moderatorsList_, /*stretch=*/1);
    layout->addWidget(loginEdit_);
    layout->addWidget(promoteButton_);
    layout->addWidget(demoteButton_);
    layout->addWidget(statusLabel_);
}

void ModeratorsDialog::setCommunity(qint64 id, const QString& name) {
    communityId_ = id;
    setWindowTitle(tr("Moderators — %1").arg(name));
    statusLabel_->clear();
}

void ModeratorsDialog::setModerators(const QStringList& logins) {
    moderatorsList_->clear();
    moderatorsList_->addItems(logins);
}

}  // namespace devicehub
