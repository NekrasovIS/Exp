#pragma once

#include <QDialog>
#include <QStringList>

class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;

namespace devicehub {

/**
 * @brief Manage-moderators dialog opened from CommunitiesPanel's
 *        owner-only "Manage Moderators…" context menu entry (issue #114).
 *
 * Pure presentation — MainWindow owns ChatRestClient and all the
 * wiring, the same "dumb widget" pattern as ProfileDialog/SettingsDialog.
 * setModerators() re-populates the list from scratch each time (after
 * every promote/demote round-trips through the server) rather than
 * this class tracking membership itself.
 */
class ModeratorsDialog : public QDialog {
    Q_OBJECT

public:
    explicit ModeratorsDialog(QWidget* parent = nullptr);

    /// Which community this dialog is currently showing — MainWindow
    /// sets this before show()ing the dialog and reads it back when
    /// wiring promoteRequested()/demoteRequested() to ChatRestClient.
    void setCommunity(qint64 id, const QString& name);
    [[nodiscard]] qint64 communityId() const { return communityId_; }

    /// Replaces the moderator list contents.
    void setModerators(const QStringList& logins);

    [[nodiscard]] QListWidget* moderatorsList() const { return moderatorsList_; }
    [[nodiscard]] QLineEdit* loginEdit() const { return loginEdit_; }
    [[nodiscard]] QPushButton* promoteButton() const { return promoteButton_; }
    [[nodiscard]] QPushButton* demoteButton() const { return demoteButton_; }
    [[nodiscard]] QLabel* statusLabel() const { return statusLabel_; }

signals:
    /// @p communityId is always communityId() — included so a caller
    /// doesn't need to separately track which community this dialog
    /// last opened for.
    void promoteRequested(qint64 communityId, const QString& login);
    void demoteRequested(qint64 communityId, const QString& login);

private:
    QListWidget* moderatorsList_ = nullptr;
    QLineEdit* loginEdit_ = nullptr;
    QPushButton* promoteButton_ = nullptr;
    QPushButton* demoteButton_ = nullptr;
    QLabel* statusLabel_ = nullptr;
    qint64 communityId_ = -1;
};

}  // namespace devicehub
