#include "ui/CommunitiesPanel.h"

#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace devicehub {

CommunitiesPanel::CommunitiesPanel(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_StyledBackground, true);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);

    auto* title = new QLabel(tr("Communities"), this);
    title->setProperty("sectionTitle", true);

    nameEdit_ = new QLineEdit(this);
    nameEdit_->setObjectName(QStringLiteral("communityNameEdit"));
    nameEdit_->setPlaceholderText(tr("New community name"));

    createButton_ = new QPushButton(tr("Create community"), this);
    createButton_->setObjectName(QStringLiteral("createCommunityButton"));
    createButton_->setProperty("accent", true);

    communityCombo_ = new QComboBox(this);
    communityCombo_->setObjectName(QStringLiteral("communityCombo"));

    refreshButton_ = new QPushButton(tr("Refresh communities"), this);
    refreshButton_->setObjectName(QStringLiteral("refreshCommunitiesButton"));

    joinButton_ = new QPushButton(tr("Join selected community"), this);
    joinButton_->setObjectName(QStringLiteral("joinCommunityButton"));
    joinButton_->setProperty("accent", true);

    layout->addWidget(title);
    layout->addWidget(nameEdit_);
    layout->addWidget(createButton_);
    layout->addWidget(communityCombo_);
    layout->addWidget(refreshButton_);
    layout->addWidget(joinButton_);
    layout->addStretch();
}

}  // namespace devicehub
