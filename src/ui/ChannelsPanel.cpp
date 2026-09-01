#include "ui/ChannelsPanel.h"

#include <QCheckBox>
#include <QColor>
#include <QDialog>
#include <QDialogButtonBox>
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
#include <QStackedWidget>
#include <QVBoxLayout>

#include "ui/IconFactory.h"
#include "ui/Theme.h"

namespace devicehub {

namespace {
constexpr int kIdRole = Qt::UserRole;
constexpr int kOwnerRole = Qt::UserRole + 1;
constexpr int kIconButtonSize = 28;
constexpr int kIconSize = 14;
constexpr int kListPageIndex = 0;
constexpr int kEmptyStatePageIndex = 1;
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

    auto* emptyState = new QWidget(this);
    emptyState->setObjectName(QStringLiteral("channelListEmptyState"));
    auto* emptyStateLayout = new QVBoxLayout(emptyState);
    emptyStateLayout->setSpacing(ui_theme::kSpacingSm);
    emptyStateLayout->addStretch();
    auto* emptyStateTitle = new QLabel(tr("No channels yet"), emptyState);
    emptyStateTitle->setAlignment(Qt::AlignCenter);
    emptyStateTitle->setWordWrap(true);
    auto* emptyStateDescription = new QLabel(tr("Create one to start chatting."), emptyState);
    emptyStateDescription->setObjectName(QStringLiteral("mutedDescription"));
    emptyStateDescription->setAlignment(Qt::AlignCenter);
    emptyStateDescription->setWordWrap(true);
    auto* emptyStateButton = new QPushButton(tr("Create channel"), emptyState);
    emptyStateButton->setObjectName(QStringLiteral("emptyStateCreateChannelButton"));
    emptyStateButton->setProperty("accent", true);
    emptyStateLayout->addWidget(emptyStateTitle);
    emptyStateLayout->addWidget(emptyStateDescription);
    emptyStateLayout->addWidget(emptyStateButton, /*stretch=*/0, Qt::AlignHCenter);
    emptyStateLayout->addStretch();
    connect(emptyStateButton, &QPushButton::clicked, this, &ChannelsPanel::showAddDialog);

    listStack_ = new QStackedWidget(this);
    listStack_->insertWidget(kListPageIndex, listWidget_);
    listStack_->insertWidget(kEmptyStatePageIndex, emptyState);
    listStack_->setCurrentIndex(kEmptyStatePageIndex);

    layout->addLayout(header);
    layout->addWidget(listStack_, /*stretch=*/1);

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
    listStack_->setCurrentIndex(channels.isEmpty() ? kEmptyStatePageIndex : kListPageIndex);
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
    // У обычного QInputDialog::getText() (прежняя реализация) нет места
    // для второго элемента управления, поэтому для чекбокса "encrypted"
    // (issue #138) вместо него нужен небольшой отдельный диалог.
    QDialog dialog(this);
    dialog.setWindowTitle(tr("New channel"));

    auto* layout = new QVBoxLayout(&dialog);
    layout->addWidget(new QLabel(tr("Channel name:"), &dialog));
    auto* nameEdit = new QLineEdit(&dialog);
    nameEdit->setObjectName(QStringLiteral("newChannelNameEdit"));
    layout->addWidget(nameEdit);

    auto* encryptedCheckBox = new QCheckBox(tr("Encrypted channel"), &dialog);
    encryptedCheckBox->setObjectName(QStringLiteral("newChannelEncryptedCheckBox"));
    encryptedCheckBox->setToolTip(
        tr("Message bodies are encrypted on this device before sending — the server never sees the "
           "plaintext. File attachments and server-side search aren't available in encrypted channels yet."));
    layout->addWidget(encryptedCheckBox);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    if (dialog.exec() == QDialog::Accepted && !nameEdit->text().trimmed().isEmpty()) {
        emit createRequested(nameEdit->text().trimmed(), encryptedCheckBox->isChecked());
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
