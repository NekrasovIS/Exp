#include "ui/PinnedMessagesDialog.h"

#include <QListWidget>
#include <QListWidgetItem>
#include <QVBoxLayout>

#include "ui/Theme.h"

namespace devicehub {

PinnedMessagesDialog::PinnedMessagesDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Pinned Messages"));

    auto* rootLayout = new QVBoxLayout(this);
    // Issue #419: недостающий setSpacing() — тот же ui_theme::kSpacingSm,
    // что уже использует SearchDialog для своего rootLayout.
    rootLayout->setSpacing(ui_theme::kSpacingSm);

    pinnedList_ = new QListWidget(this);
    pinnedList_->setObjectName(QStringLiteral("pinnedMessagesList"));
    connect(pinnedList_, &QListWidget::itemActivated, this, [this](QListWidgetItem* item) {
        emit messageActivated(item->data(Qt::UserRole).toLongLong());
    });

    rootLayout->addWidget(pinnedList_, /*stretch=*/1);

    resize(420, 360);
}

void PinnedMessagesDialog::setPinnedMessages(const QList<PinnedMessageInfo>& pinned) {
    pinnedList_->clear();
    for (const PinnedMessageInfo& message : pinned) {
        auto* item =
            new QListWidgetItem(tr("%1: %2\n📌 by %3").arg(message.author, message.body, message.pinnedBy));
        item->setData(Qt::UserRole, message.id);
        pinnedList_->addItem(item);
    }
}

}  // namespace devicehub
