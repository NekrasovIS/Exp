#pragma once

#include <QWidget>

#include "chat/ChatRestClient.h"

class QListWidget;
class QPoint;
class QPushButton;

namespace devicehub {

/**
 * @brief Top-left sidebar section: a list of communities plus a "+"
 *        button to create one and a right-click menu to join/rename/
 *        delete.
 *
 * Pure presentation — owns no network state. MainWindow feeds it the
 * current community list and the signed-in user's login (so it can
 * decide whether to offer rename/delete for a given item — those are
 * owner-only server-side too, this just avoids showing actions that
 * would only come back as a 403) and reacts to the request signals by
 * calling ChatRestClient itself.
 */
class CommunitiesPanel : public QWidget {
    Q_OBJECT

public:
    explicit CommunitiesPanel(QWidget* parent = nullptr);

    /// Replaces the list contents.
    void setCommunities(const QList<ChatItem>& communities);

    /// Selects the item with @p id, if present, without emitting
    /// communitySelected() — used to reflect a selection MainWindow
    /// already decided on (e.g. auto-selecting a freshly created
    /// community) without re-triggering the same request.
    void selectCommunityId(qint64 id);

    /// Needed to decide whether to offer rename/delete for an item.
    void setCurrentUserLogin(const QString& login);

    [[nodiscard]] QListWidget* listWidget() const { return listWidget_; }
    [[nodiscard]] QPushButton* addButton() const { return addButton_; }
    [[nodiscard]] QPushButton* refreshButton() const { return refreshButton_; }

signals:
    void createRequested(const QString& name);
    void renameRequested(qint64 id, const QString& newName);
    void deleteRequested(qint64 id);
    void joinRequested(qint64 id);
    void communitySelected(qint64 id);

private:
    void showAddDialog();
    void showContextMenu(const QPoint& pos);

    QListWidget* listWidget_ = nullptr;
    QPushButton* addButton_ = nullptr;
    QPushButton* refreshButton_ = nullptr;
    QString currentUserLogin_;
};

}  // namespace devicehub
