#include "ui/PinnedMessagesDialog.h"

#include <QColor>
#include <QListWidget>
#include <QListWidgetItem>
#include <QVBoxLayout>

#include "ui/IconFactory.h"
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
        // Issue #418: значок канцелярской кнопки — в отдельном слоте
        // QListWidgetItem::setIcon(), а не сырым эмодзи внутри текста
        // (раньше "📌" стоял посреди второй строки).
        auto* item = new QListWidgetItem(tr("%1: %2\nby %3").arg(message.author, message.body, message.pinnedBy));
        item->setIcon(ui_icons::pinIcon(QColor(ui_theme::kMutedForeground)));
        item->setData(Qt::UserRole, message.id);
        pinnedList_->addItem(item);
    }
}

}  // namespace devicehub
