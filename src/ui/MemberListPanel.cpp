#include "ui/MemberListPanel.h"

#include <QAction>
#include <QFrame>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMenu>
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
    listWidget_->setContextMenuPolicy(Qt::CustomContextMenu);

    layout->addWidget(titleLabel_);
    layout->addWidget(listWidget_, /*stretch=*/1);

    connect(listWidget_, &QListWidget::customContextMenuRequested, this, &MemberListPanel::showContextMenu);

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
        item->setIcon(ui_icons::memberAvatarIcon(login.left(1).toUpper(), onlineLogins_.contains(login)));
    }
}

void MemberListPanel::setOnlineLogins(const QStringList& logins) {
    onlineLogins_ = QSet<QString>(logins.begin(), logins.end());
    for (int i = 0; i < listWidget_->count(); ++i) {
        QListWidgetItem* item = listWidget_->item(i);
        item->setIcon(ui_icons::memberAvatarIcon(item->text().left(1).toUpper(), onlineLogins_.contains(item->text())));
    }
}

void MemberListPanel::setLoginOnline(const QString& login, bool online) {
    if (online) {
        onlineLogins_.insert(login);
    } else {
        onlineLogins_.remove(login);
    }
    for (int i = 0; i < listWidget_->count(); ++i) {
        QListWidgetItem* item = listWidget_->item(i);
        if (item->text() == login) {
            item->setIcon(ui_icons::memberAvatarIcon(login.left(1).toUpper(), online));
            break;
        }
    }
}

void MemberListPanel::setCurrentUserLogin(const QString& login) {
    currentUserLogin_ = login;
}

void MemberListPanel::setChannelEncrypted(bool encrypted) {
    channelEncrypted_ = encrypted;
}

void MemberListPanel::showContextMenu(const QPoint& pos) {
    QListWidgetItem* item = listWidget_->itemAt(pos);
    if (item == nullptr || !channelEncrypted_) {
        return;
    }
    const QString login = item->text();
    if (login == currentUserLogin_) {
        return;  // Себе самому выдавать доступ бессмысленно — он уже есть.
    }

    QMenu menu(this);
    QAction* grantKeyAction = menu.addAction(tr("Grant channel key access"));

    QAction* chosen = menu.exec(listWidget_->mapToGlobal(pos));
    if (chosen == grantKeyAction) {
        emit grantChannelKeyAccessRequested(login);
    }
}

}  // namespace devicehub
