#include "ui/MemberListPanel.h"

#include <QAction>
#include <QFrame>
#include <QImage>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMenu>
#include <QSize>
#include <QVBoxLayout>

#include <algorithm>
#include <optional>

#include "ui/IconFactory.h"
#include "ui/Theme.h"
#include "user/AvatarCache.h"

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
        applyAvatarIcon(item, login);
    }
}

void MemberListPanel::setOnlineLogins(const QStringList& logins) {
    onlineLogins_ = QSet<QString>(logins.begin(), logins.end());
    for (int i = 0; i < listWidget_->count(); ++i) {
        QListWidgetItem* item = listWidget_->item(i);
        applyAvatarIcon(item, item->text());
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
            applyAvatarIcon(item, login);
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

void MemberListPanel::setAvatarCache(AvatarCache* cache) {
    avatarCache_ = cache;
    if (avatarCache_ != nullptr) {
        connect(avatarCache_, &AvatarCache::avatarReady, this, [this](const QString& login) {
            for (int i = 0; i < listWidget_->count(); ++i) {
                QListWidgetItem* item = listWidget_->item(i);
                if (item->text() == login) {
                    applyAvatarIcon(item, login);
                    break;
                }
            }
        });
    }
}

void MemberListPanel::applyAvatarIcon(QListWidgetItem* item, const QString& login) {
    const bool online = onlineLogins_.contains(login);
    if (avatarCache_ != nullptr) {
        if (const std::optional<QImage> image = avatarCache_->imageFor(login); image.has_value()) {
            item->setIcon(ui_icons::realMemberAvatarIcon(*image, online));
            return;
        }
    }
    item->setIcon(ui_icons::memberAvatarIcon(login.left(1).toUpper(), online));
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
