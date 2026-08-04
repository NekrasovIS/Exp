#include "ui/CommunitiesPanel.h"

#include <QColor>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPoint>
#include <QPushButton>
#include <QSize>
#include <QVBoxLayout>

#include "ui/IconFactory.h"

namespace devicehub {

namespace {
constexpr int kIdRole = Qt::UserRole;
constexpr int kOwnerRole = Qt::UserRole + 1;
constexpr int kIconButtonSize = 28;
constexpr int kIconSize = 14;
}  // namespace

CommunitiesPanel::CommunitiesPanel(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_StyledBackground, true);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);

    auto* header = new QHBoxLayout;
    auto* title = new QLabel(tr("Communities"), this);
    title->setProperty("sectionTitle", true);

    refreshButton_ = new QPushButton(this);
    refreshButton_->setObjectName(QStringLiteral("refreshCommunitiesButton"));
    refreshButton_->setToolTip(tr("Refresh communities"));
    refreshButton_->setIcon(ui_icons::refreshIcon());
    refreshButton_->setIconSize(QSize(kIconSize, kIconSize));
    refreshButton_->setFixedSize(kIconButtonSize, kIconButtonSize);
    refreshButton_->setProperty("iconOnly", true);

    addButton_ = new QPushButton(this);
    addButton_->setObjectName(QStringLiteral("createCommunityButton"));
    addButton_->setToolTip(tr("Create community"));
    addButton_->setProperty("accent", true);
    addButton_->setIcon(ui_icons::plusIcon(QColor("#ffffff")));
    addButton_->setIconSize(QSize(kIconSize, kIconSize));
    addButton_->setFixedSize(kIconButtonSize, kIconButtonSize);
    addButton_->setProperty("iconOnly", true);

    header->addWidget(title);
    header->addStretch();
    header->addWidget(refreshButton_);
    header->addWidget(addButton_);

    listWidget_ = new QListWidget(this);
    listWidget_->setObjectName(QStringLiteral("communityList"));
    listWidget_->setContextMenuPolicy(Qt::CustomContextMenu);

    layout->addLayout(header);
    layout->addWidget(listWidget_, /*stretch=*/1);

    connect(addButton_, &QPushButton::clicked, this, &CommunitiesPanel::showAddDialog);
    connect(listWidget_, &QListWidget::customContextMenuRequested, this, &CommunitiesPanel::showContextMenu);
    connect(listWidget_, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        emit communitySelected(item->data(kIdRole).toLongLong());
    });
}

void CommunitiesPanel::setCommunities(const QList<ChatItem>& communities) {
    listWidget_->clear();
    for (const ChatItem& community : communities) {
        auto* item = new QListWidgetItem(community.name, listWidget_);
        item->setData(kIdRole, community.id);
        item->setData(kOwnerRole, community.ownerLogin);
    }
}

void CommunitiesPanel::selectCommunityId(qint64 id) {
    for (int row = 0; row < listWidget_->count(); ++row) {
        if (QListWidgetItem* item = listWidget_->item(row); item->data(kIdRole).toLongLong() == id) {
            listWidget_->setCurrentItem(item);
            return;
        }
    }
}

void CommunitiesPanel::setCurrentUserLogin(const QString& login) {
    currentUserLogin_ = login;
}

void CommunitiesPanel::showAddDialog() {
    bool ok = false;
    const QString name =
        QInputDialog::getText(this, tr("New community"), tr("Community name:"), QLineEdit::Normal, QString(), &ok);
    if (ok && !name.trimmed().isEmpty()) {
        emit createRequested(name.trimmed());
    }
}

void CommunitiesPanel::showContextMenu(const QPoint& pos) {
    QListWidgetItem* item = listWidget_->itemAt(pos);
    if (item == nullptr) {
        return;
    }

    const qint64 id = item->data(kIdRole).toLongLong();
    const bool isOwner = !currentUserLogin_.isEmpty() && item->data(kOwnerRole).toString() == currentUserLogin_;

    QMenu menu(this);
    QAction* joinAction = menu.addAction(tr("Join"));
    QAction* renameAction = isOwner ? menu.addAction(tr("Rename…")) : nullptr;
    QAction* deleteAction = isOwner ? menu.addAction(tr("Delete")) : nullptr;

    QAction* chosen = menu.exec(listWidget_->mapToGlobal(pos));
    if (chosen == nullptr) {
        return;
    }

    if (chosen == joinAction) {
        emit joinRequested(id);
    } else if (chosen == renameAction) {
        bool ok = false;
        const QString newName =
            QInputDialog::getText(this, tr("Rename community"), tr("New name:"), QLineEdit::Normal, item->text(), &ok);
        if (ok && !newName.trimmed().isEmpty()) {
            emit renameRequested(id, newName.trimmed());
        }
    } else if (chosen == deleteAction) {
        if (QMessageBox::question(this, tr("Delete community"),
                                   tr("Delete '%1' and all of its channels? This can't be undone.").arg(item->text())) ==
            QMessageBox::Yes) {
            emit deleteRequested(id);
        }
    }
}

}  // namespace devicehub
