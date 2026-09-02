#include "ui/MemberListPanel.h"

#include <QFrame>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QSize>
#include <QVBoxLayout>

#include <algorithm>

#include "ui/IconFactory.h"
#include "ui/Theme.h"

namespace devicehub {

namespace {
constexpr int kAvatarIconSize = 28;
}  // namespace

MemberListPanel::MemberListPanel(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_StyledBackground, true);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(ui_theme::kSpacingSm, ui_theme::kSpacingSm, ui_theme::kSpacingSm,
                                ui_theme::kSpacingSm);
    layout->setSpacing(ui_theme::kSpacingSm);

    titleLabel_ = new QLabel(this);
    titleLabel_->setObjectName(QStringLiteral("memberListTitle"));
    titleLabel_->setProperty("sectionTitle", true);

    listWidget_ = new QListWidget(this);
    listWidget_->setObjectName(QStringLiteral("memberList"));
    listWidget_->setFrameShape(QFrame::NoFrame);
    listWidget_->setIconSize(QSize(kAvatarIconSize, kAvatarIconSize));

    layout->addWidget(titleLabel_);
    layout->addWidget(listWidget_, /*stretch=*/1);

    setMembers({});
}

void MemberListPanel::setMembers(const QStringList& logins) {
    QStringList sorted = logins;
    std::sort(sorted.begin(), sorted.end(),
              [](const QString& a, const QString& b) { return a.compare(b, Qt::CaseInsensitive) < 0; });

    // ЗАГЛАВНЫМИ с числом участников — заголовок раздела читается
    // заметнее (issue #182).
    titleLabel_->setText(tr("MEMBERS — %1").arg(sorted.size()));

    listWidget_->clear();
    for (const QString& login : sorted) {
        auto* item = new QListWidgetItem(login, listWidget_);
        item->setIcon(ui_icons::communityAvatarIcon(login.left(1).toUpper()));
    }
}

}  // namespace devicehub
