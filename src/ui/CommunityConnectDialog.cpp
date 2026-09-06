#include "ui/CommunityConnectDialog.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include "ui/Theme.h"

namespace devicehub {

CommunityConnectDialog::CommunityConnectDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Add a Community"));

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setSpacing(ui_theme::kSpacingMd);

    auto* joinLabel = new QLabel(tr("Join with an invite code"), this);
    joinLabel->setProperty("sectionTitle", true);
    auto* joinRow = new QHBoxLayout;
    joinRow->setSpacing(ui_theme::kSpacingSm);
    inviteCodeEdit_ = new QLineEdit(this);
    inviteCodeEdit_->setObjectName(QStringLiteral("communityInviteCodeEdit"));
    inviteCodeEdit_->setPlaceholderText(tr("Invite code"));
    connect(inviteCodeEdit_, &QLineEdit::returnPressed, this, &CommunityConnectDialog::onJoinClicked);
    joinButton_ = new QPushButton(tr("Join"), this);
    joinButton_->setObjectName(QStringLiteral("communityJoinButton"));
    joinButton_->setProperty("accent", true);
    connect(joinButton_, &QPushButton::clicked, this, &CommunityConnectDialog::onJoinClicked);
    joinRow->addWidget(inviteCodeEdit_, /*stretch=*/1);
    joinRow->addWidget(joinButton_);

    auto* separator = new QFrame(this);
    separator->setFrameShape(QFrame::HLine);

    auto* createLabel = new QLabel(tr("Or create a new one"), this);
    createLabel->setProperty("sectionTitle", true);
    auto* createRow = new QHBoxLayout;
    createRow->setSpacing(ui_theme::kSpacingSm);
    nameEdit_ = new QLineEdit(this);
    nameEdit_->setObjectName(QStringLiteral("communityNameEdit"));
    nameEdit_->setPlaceholderText(tr("Community name"));
    connect(nameEdit_, &QLineEdit::returnPressed, this, &CommunityConnectDialog::onCreateClicked);
    createButton_ = new QPushButton(tr("Create"), this);
    createButton_->setObjectName(QStringLiteral("communityCreateButton"));
    connect(createButton_, &QPushButton::clicked, this, &CommunityConnectDialog::onCreateClicked);
    createRow->addWidget(nameEdit_, /*stretch=*/1);
    createRow->addWidget(createButton_);

    rootLayout->addWidget(joinLabel);
    rootLayout->addLayout(joinRow);
    rootLayout->addWidget(separator);
    rootLayout->addWidget(createLabel);
    rootLayout->addLayout(createRow);

    resize(360, 180);
}

void CommunityConnectDialog::onJoinClicked() {
    const QString code = inviteCodeEdit_->text().trimmed();
    if (code.isEmpty()) {
        return;
    }
    emit joinRequested(code);
    inviteCodeEdit_->clear();
    accept();
}

void CommunityConnectDialog::onCreateClicked() {
    const QString name = nameEdit_->text().trimmed();
    if (name.isEmpty()) {
        return;
    }
    emit createRequested(name);
    nameEdit_->clear();
    accept();
}

}  // namespace devicehub
