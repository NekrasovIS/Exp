#include "ui/ChannelsPanel.h"

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
#include "ui/Theme.h"

namespace devicehub {

namespace {
constexpr int kIdRole = Qt::UserRole;
constexpr int kOwnerRole = Qt::UserRole + 1;
constexpr int kIconButtonSize = 28;
constexpr int kIconSize = 14;
}  // namespace

ChannelsPanel::ChannelsPanel(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_StyledBackground, true);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(ui_theme::kSpacingSm, ui_theme::kSpacingSm, ui_theme::kSpacingSm,
                                ui_theme::kSpacingSm);
    layout->setSpacing(ui_theme::kSpacingSm);

    auto* header = new QHBoxLayout;
    header->setSpacing(ui_theme::kSpacingSm);
    auto* title = new QLabel(tr("Channels"), this);
    title->setProperty("sectionTitle", true);

    refreshButton_ = new QPushButton(this);
    refreshButton_->setObjectName(QStringLiteral("refreshChannelsButton"));
    refreshButton_->setToolTip(tr("Refresh channels"));
    refreshButton_->setIcon(ui_icons::refreshIcon());
    refreshButton_->setIconSize(QSize(kIconSize, kIconSize));
    refreshButton_->setFixedSize(kIconButtonSize, kIconButtonSize);
    refreshButton_->setProperty("iconOnly", true);

    addButton_ = new QPushButton(this);
    addButton_->setObjectName(QStringLiteral("createChannelButton"));
    addButton_->setToolTip(tr("Create channel"));
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
    listWidget_->setObjectName(QStringLiteral("channelList"));
    listWidget_->setContextMenuPolicy(Qt::CustomContextMenu);

    layout->addLayout(header);
    layout->addWidget(listWidget_, /*stretch=*/1);

    connect(addButton_, &QPushButton::clicked, this, &ChannelsPanel::showAddDialog);
    connect(listWidget_, &QListWidget::customContextMenuRequested, this, &ChannelsPanel::showContextMenu);
    connect(listWidget_, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        emit channelSelected(item->data(kIdRole).toLongLong(), item->text());
    });
}

void ChannelsPanel::setChannels(const QList<ChatItem>& channels) {
    listWidget_->clear();
    for (const ChatItem& channel : channels) {
        auto* item = new QListWidgetItem(channel.name, listWidget_);
        item->setData(kIdRole, channel.id);
        item->setData(kOwnerRole, channel.ownerLogin);
    }
}

void ChannelsPanel::selectChannelId(qint64 id) {
    for (int row = 0; row < listWidget_->count(); ++row) {
        if (QListWidgetItem* item = listWidget_->item(row); item->data(kIdRole).toLongLong() == id) {
            listWidget_->setCurrentItem(item);
            return;
        }
    }
}

void ChannelsPanel::setCurrentUserLogin(const QString& login) {
    currentUserLogin_ = login;
}

void ChannelsPanel::showAddDialog() {
    bool ok = false;
    const QString name =
        QInputDialog::getText(this, tr("New channel"), tr("Channel name:"), QLineEdit::Normal, QString(), &ok);
    if (ok && !name.trimmed().isEmpty()) {
        emit createRequested(name.trimmed());
    }
}

void ChannelsPanel::showContextMenu(const QPoint& pos) {
    QListWidgetItem* item = listWidget_->itemAt(pos);
    if (item == nullptr) {
        return;
    }

    const qint64 id = item->data(kIdRole).toLongLong();
    const bool isOwner = !currentUserLogin_.isEmpty() && item->data(kOwnerRole).toString() == currentUserLogin_;
    if (!isOwner) {
        return;
    }

    QMenu menu(this);
    QAction* renameAction = menu.addAction(tr("Rename…"));
    QAction* deleteAction = menu.addAction(tr("Delete"));

    QAction* chosen = menu.exec(listWidget_->mapToGlobal(pos));
    if (chosen == renameAction) {
        bool ok = false;
        const QString newName =
            QInputDialog::getText(this, tr("Rename channel"), tr("New name:"), QLineEdit::Normal, item->text(), &ok);
        if (ok && !newName.trimmed().isEmpty()) {
            emit renameRequested(id, newName.trimmed());
        }
    } else if (chosen == deleteAction) {
        if (QMessageBox::question(this, tr("Delete channel"),
                                   tr("Delete '%1' and all of its messages? This can't be undone.").arg(item->text())) ==
            QMessageBox::Yes) {
            emit deleteRequested(id);
        }
    }
}

}  // namespace devicehub
