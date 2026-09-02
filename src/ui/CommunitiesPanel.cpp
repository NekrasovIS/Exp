#include "ui/CommunitiesPanel.h"

#include <QColor>
#include <QInputDialog>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPoint>
#include <QPushButton>
#include <QSize>
#include <QVBoxLayout>

#include "ui/IconFactory.h"
#include "ui/Theme.h"

namespace devicehub {

namespace {
constexpr int kIdRole = Qt::UserRole;
constexpr int kOwnerRole = Qt::UserRole + 1;
constexpr int kNameRole = Qt::UserRole + 2;
// Метрики ближе к иконочной полосе серверов Discord (issue: визуальный
// проход по мотивам Discord — расположение/размеры, не цвета) — более
// широкая полоса с более крупными круглыми аватарами вместо прежней
// тесной раскладки.
constexpr int kRailWidth = 72;
constexpr int kIconButtonSize = 32;
constexpr int kIconButtonIconSize = 16;
constexpr int kAvatarIconSize = 44;
constexpr int kAvatarGridSize = 56;
}  // namespace

CommunitiesPanel::CommunitiesPanel(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_StyledBackground, true);
    setFixedWidth(kRailWidth);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(ui_theme::kSpacingSm, ui_theme::kSpacingSm, ui_theme::kSpacingSm,
                                ui_theme::kSpacingSm);
    layout->setSpacing(ui_theme::kSpacingSm);

    refreshButton_ = new QPushButton(this);
    refreshButton_->setObjectName(QStringLiteral("refreshCommunitiesButton"));
    refreshButton_->setToolTip(tr("Refresh communities"));
    refreshButton_->setIcon(ui_icons::refreshIcon());
    refreshButton_->setIconSize(QSize(kIconButtonIconSize, kIconButtonIconSize));
    refreshButton_->setFixedSize(kIconButtonSize, kIconButtonSize);
    refreshButton_->setProperty("iconOnly", true);

    listWidget_ = new QListWidget(this);
    listWidget_->setObjectName(QStringLiteral("communityList"));
    listWidget_->setContextMenuPolicy(Qt::CustomContextMenu);
    listWidget_->setViewMode(QListView::IconMode);
    listWidget_->setFlow(QListView::TopToBottom);
    listWidget_->setWrapping(false);
    listWidget_->setMovement(QListView::Static);
    listWidget_->setUniformItemSizes(true);
    listWidget_->setResizeMode(QListView::Adjust);
    listWidget_->setIconSize(QSize(kAvatarIconSize, kAvatarIconSize));
    listWidget_->setGridSize(QSize(kAvatarGridSize, kAvatarGridSize));
    listWidget_->setFrameShape(QFrame::NoFrame);
    listWidget_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    addButton_ = new QPushButton(this);
    addButton_->setObjectName(QStringLiteral("createCommunityButton"));
    addButton_->setToolTip(tr("Create community"));
    addButton_->setProperty("accent", true);
    addButton_->setIcon(ui_icons::plusIcon(QColor(ui_theme::kAccentForeground)));
    addButton_->setIconSize(QSize(kIconButtonIconSize, kIconButtonIconSize));
    addButton_->setFixedSize(kIconButtonSize, kIconButtonSize);
    addButton_->setProperty("iconOnly", true);

    layout->addWidget(refreshButton_, /*stretch=*/0, Qt::AlignHCenter);
    layout->addWidget(listWidget_, /*stretch=*/1);
    layout->addWidget(addButton_, /*stretch=*/0, Qt::AlignHCenter);

    connect(addButton_, &QPushButton::clicked, this, &CommunitiesPanel::showAddDialog);
    connect(listWidget_, &QListWidget::customContextMenuRequested, this, &CommunitiesPanel::showContextMenu);
    connect(listWidget_, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        emit communitySelected(item->data(kIdRole).toLongLong());
    });
}

void CommunitiesPanel::setCommunities(const QList<ChatItem>& communities) {
    listWidget_->clear();
    for (const ChatItem& community : communities) {
        auto* item = new QListWidgetItem(listWidget_);
        item->setIcon(ui_icons::communityAvatarIcon(community.name.left(1).toUpper()));
        item->setToolTip(community.name);
        item->setData(kIdRole, community.id);
        item->setData(kOwnerRole, community.ownerLogin);
        item->setData(kNameRole, community.name);
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
    const QString name = item->data(kNameRole).toString();
    const bool isOwner = !currentUserLogin_.isEmpty() && item->data(kOwnerRole).toString() == currentUserLogin_;

    QMenu menu(this);
    QAction* joinAction = menu.addAction(tr("Join"));
    QAction* renameAction = isOwner ? menu.addAction(tr("Rename…")) : nullptr;
    QAction* deleteAction = isOwner ? menu.addAction(tr("Delete")) : nullptr;
    QAction* manageModeratorsAction = isOwner ? menu.addAction(tr("Manage Moderators…")) : nullptr;

    QAction* chosen = menu.exec(listWidget_->mapToGlobal(pos));
    if (chosen == nullptr) {
        return;
    }

    if (chosen == joinAction) {
        emit joinRequested(id);
    } else if (chosen == renameAction) {
        bool ok = false;
        const QString newName =
            QInputDialog::getText(this, tr("Rename community"), tr("New name:"), QLineEdit::Normal, name, &ok);
        if (ok && !newName.trimmed().isEmpty()) {
            emit renameRequested(id, newName.trimmed());
        }
    } else if (chosen == deleteAction) {
        if (QMessageBox::question(this, tr("Delete community"),
                                   tr("Delete '%1' and all of its channels? This can't be undone.").arg(name)) ==
            QMessageBox::Yes) {
            emit deleteRequested(id);
        }
    } else if (chosen == manageModeratorsAction) {
        emit manageModeratorsRequested(id, name);
    }
}

}  // namespace devicehub
