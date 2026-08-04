#include "ui/FooterBar.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

namespace devicehub {

FooterBar::FooterBar(QWidget* parent) : QWidget(parent) {
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 4, 8, 4);

    profileLabel_ = new QLabel(tr("Not signed in"), this);
    profileLabel_->setObjectName(QStringLiteral("footerProfileLabel"));

    settingsButton_ = new QPushButton(tr("⚙ Settings"), this);
    settingsButton_->setObjectName(QStringLiteral("footerSettingsButton"));

    layout->addWidget(profileLabel_);
    layout->addWidget(settingsButton_);
    layout->addStretch();
}

void FooterBar::setProfileText(const QString& text) {
    profileLabel_->setText(text);
}

}  // namespace devicehub
