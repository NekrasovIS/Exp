#include "ui/FriendsPanel.h"

#include <QAbstractAnimation>
#include <QColor>
#include <QEasingCurve>
#include <QEvent>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPoint>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QSize>
#include <QVBoxLayout>

#include "ui/IconFactory.h"
#include "ui/Theme.h"

namespace devicehub {

namespace {
constexpr int kRequestIdRole = Qt::UserRole;
constexpr int kIconButtonSize = 28;
constexpr int kIconSize = 14;
constexpr int kSlideDurationMs = 200;
}  // namespace

FriendsPanel::FriendsPanel(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_StyledBackground, true);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(ui_theme::kSpacingSm, ui_theme::kSpacingSm, ui_theme::kSpacingSm,
                                ui_theme::kSpacingSm);
    layout->setSpacing(ui_theme::kSpacingSm);

    auto* header = new QHBoxLayout;
    header->setSpacing(ui_theme::kSpacingSm);

    auto* title = new QLabel(tr("Friends"), this);
    title->setProperty("sectionTitle", true);

    addFriendButton_ = new QPushButton(this);
    addFriendButton_->setObjectName(QStringLiteral("addFriendButton"));
    addFriendButton_->setToolTip(tr("Add friend"));
    addFriendButton_->setProperty("accent", true);
    addFriendButton_->setIcon(ui_icons::plusIcon(QColor(ui_theme::kAccentForeground)));
    addFriendButton_->setIconSize(QSize(kIconSize, kIconSize));
    addFriendButton_->setFixedSize(kIconButtonSize, kIconButtonSize);
    addFriendButton_->setProperty("iconOnly", true);

    header->addWidget(title);
    header->addStretch();
    header->addWidget(addFriendButton_);

    auto* requestsTitle = new QLabel(tr("Pending requests"), this);
    requestsTitle->setObjectName(QStringLiteral("mutedDescription"));

    requestsList_ = new QListWidget(this);
    requestsList_->setObjectName(QStringLiteral("friendRequestsList"));
    requestsList_->setContextMenuPolicy(Qt::CustomContextMenu);
    // Обычно короткий список — не должен отбирать место у списка
    // друзей ниже, растёт только до содержимого.
    requestsList_->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
    requestsList_->setMaximumHeight(120);

    auto* friendsTitle = new QLabel(tr("Friends"), this);
    friendsTitle->setObjectName(QStringLiteral("mutedDescription"));

    friendsList_ = new QListWidget(this);
    friendsList_->setObjectName(QStringLiteral("friendsList"));
    friendsList_->setContextMenuPolicy(Qt::CustomContextMenu);

    layout->addLayout(header);
    layout->addWidget(requestsTitle);
    layout->addWidget(requestsList_);
    layout->addWidget(friendsTitle);
    layout->addWidget(friendsList_, /*stretch=*/1);

    connect(addFriendButton_, &QPushButton::clicked, this, &FriendsPanel::showAddFriendDialog);
    connect(requestsList_, &QListWidget::customContextMenuRequested, this, &FriendsPanel::showRequestContextMenu);
    connect(friendsList_, &QListWidget::customContextMenuRequested, this, &FriendsPanel::showFriendContextMenu);
    connect(friendsList_, &QListWidget::itemClicked, this,
            [this](QListWidgetItem* item) { emit friendSelected(item->text()); });

    // Оверлей (issue #216): не участвует в layout'е родителя (тот же
    // приём, что и у ToastBanner) — сама следит за его resize() и
    // держит собственную геометрию синхронной, пока не идёт анимация.
    slideAnimation_ = new QPropertyAnimation(this, "geometry", this);
    slideAnimation_->setDuration(kSlideDurationMs);
    slideAnimation_->setEasingCurve(QEasingCurve::OutCubic);
    connect(slideAnimation_, &QPropertyAnimation::finished, this, [this]() {
        if (!open_) {
            hide();
        }
    });
    if (parent != nullptr) {
        parent->installEventFilter(this);
    }
    hide();
}

void FriendsPanel::setFriends(const QStringList& logins) {
    friendsList_->clear();
    for (const QString& login : logins) {
        new QListWidgetItem(login, friendsList_);
    }
}

void FriendsPanel::setIncomingRequests(const QList<FriendRequestInfo>& requests) {
    requestsList_->clear();
    for (const FriendRequestInfo& request : requests) {
        auto* item = new QListWidgetItem(tr("%1 wants to be friends").arg(request.requesterLogin), requestsList_);
        item->setData(kRequestIdRole, request.id);
    }
}

void FriendsPanel::setOpen(bool open) {
    if (open == open_ || parentWidget() == nullptr) {
        return;
    }
    open_ = open;

    if (open) {
        setGeometry(hiddenGeometry());
        show();
        raise();
    }
    slideAnimation_->stop();
    slideAnimation_->setStartValue(geometry());
    slideAnimation_->setEndValue(open ? visibleGeometry() : hiddenGeometry());
    slideAnimation_->start();
}

bool FriendsPanel::eventFilter(QObject* watched, QEvent* event) {
    if (watched == parentWidget() && event->type() == QEvent::Resize &&
        slideAnimation_->state() != QAbstractAnimation::Running) {
        // Держим геометрию синхронной с изменившимся размером родителя —
        // во время самой анимации не трогаем, чтобы не оборвать слайд на
        // полпути (редкий edge case, сознательно не пытаемся починить).
        setGeometry(open_ ? visibleGeometry() : hiddenGeometry());
    }
    return QWidget::eventFilter(watched, event);
}

QRect FriendsPanel::visibleGeometry() const {
    return parentWidget()->rect();
}

QRect FriendsPanel::hiddenGeometry() const {
    QRect rect = parentWidget()->rect();
    rect.moveLeft(-rect.width());
    return rect;
}

void FriendsPanel::showAddFriendDialog() {
    bool ok = false;
    const QString login =
        QInputDialog::getText(this, tr("Add friend"), tr("Their login:"), QLineEdit::Normal, QString(), &ok);
    if (ok && !login.trimmed().isEmpty()) {
        emit addFriendRequested(login.trimmed());
    }
}

void FriendsPanel::showRequestContextMenu(const QPoint& pos) {
    QListWidgetItem* item = requestsList_->itemAt(pos);
    if (item == nullptr) {
        return;
    }
    const qint64 requestId = item->data(kRequestIdRole).toLongLong();

    QMenu menu(this);
    QAction* acceptAction = menu.addAction(tr("Accept"));
    QAction* declineAction = menu.addAction(tr("Decline"));

    QAction* chosen = menu.exec(requestsList_->mapToGlobal(pos));
    if (chosen == acceptAction) {
        emit acceptRequestRequested(requestId);
    } else if (chosen == declineAction) {
        emit declineRequestRequested(requestId);
    }
}

void FriendsPanel::showFriendContextMenu(const QPoint& pos) {
    QListWidgetItem* item = friendsList_->itemAt(pos);
    if (item == nullptr) {
        return;
    }
    const QString login = item->text();

    QMenu menu(this);
    QAction* removeAction = menu.addAction(tr("Remove Friend"));

    QAction* chosen = menu.exec(friendsList_->mapToGlobal(pos));
    if (chosen == removeAction) {
        if (QMessageBox::question(this, tr("Remove friend"), tr("Remove '%1' from your friends?").arg(login)) ==
            QMessageBox::Yes) {
            emit removeFriendRequested(login);
        }
    }
}

}  // namespace devicehub
