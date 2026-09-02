#include "ui/FooterBar.h"

#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>

#include "ui/Theme.h"

namespace devicehub {

namespace {
constexpr int kAvatarDiameter = 28;
}  // namespace

FooterBar::FooterBar(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_StyledBackground, true);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(ui_theme::kSpacingSm, ui_theme::kSpacingSm, ui_theme::kSpacingSm,
                                ui_theme::kSpacingSm);
    layout->setSpacing(ui_theme::kSpacingSm);

    avatarLabel_ = new QLabel(QStringLiteral("?"), this);
    avatarLabel_->setObjectName(QStringLiteral("footerAvatar"));
    avatarLabel_->setFixedSize(kAvatarDiameter, kAvatarDiameter);
    avatarLabel_->setAlignment(Qt::AlignCenter);
    // Clickable entry point for account settings (issue #151) — stays a
    // QLabel (not a QPushButton) so its existing round-avatar styling
    // and FooterBarTest's QLabel-based lookups keep working; an event
    // filter is the least invasive way to add click handling to a
    // QLabel without subclassing it.
    avatarLabel_->setCursor(Qt::PointingHandCursor);
    avatarLabel_->installEventFilter(this);

    profileLabel_ = new QLabel(tr("Not signed in"), this);
    profileLabel_->setObjectName(QStringLiteral("footerProfileLabel"));

    settingsButton_ = new QPushButton(tr("⚙ Settings"), this);
    settingsButton_->setObjectName(QStringLiteral("footerSettingsButton"));

    layout->addWidget(avatarLabel_);
    layout->addWidget(profileLabel_);
    layout->addWidget(settingsButton_);
    layout->addStretch();
}

void FooterBar::setProfileText(const QString& text) {
    profileLabel_->setText(text);
    avatarLabel_->setText(text.isEmpty() ? QStringLiteral("?") : text.left(1).toUpper());
}

bool FooterBar::eventFilter(QObject* watched, QEvent* event) {
    if (watched == avatarLabel_ && event->type() == QEvent::MouseButtonPress) {
        if (static_cast<QMouseEvent*>(event)->button() == Qt::LeftButton) {
            emit accountSettingsRequested();
        }
        return true;
    }
    return QWidget::eventFilter(watched, event);
}

}  // namespace devicehub
