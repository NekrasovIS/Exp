#pragma once

#include <QDialog>

class QLabel;
class QLineEdit;
class QPushButton;

namespace devicehub {

struct UserProfile;
struct ProfileEdits;

/**
 * @brief Edit-profile dialog opened from AccountMenu's "Edit Profile"
 *        button (issue #110): display name, avatar URL, email (issue
 *        #156) and Telegram chat_id (issue #174) — either of the last
 *        two enables one-time-code login through that channel.
 *
 * Pure presentation — MainWindow owns UserProfileClient and all the
 * wiring: it calls setProfile() to prefill fields (kept in sync with
 * every UserProfileClient::profileReceived()/profileUpdated(), not just
 * on open) and listens for saveRequested() to actually submit the
 * change, the same "dumb widget" pattern as SettingsDialog/AccountMenu.
 */
class ProfileDialog : public QDialog {
    Q_OBJECT

public:
    explicit ProfileDialog(QWidget* parent = nullptr);

    /// Prefills the fields — a no-op on the currently-focused field, so
    /// it's safe to call while the user is mid-edit (e.g. a background
    /// refresh landing while the dialog happens to be open).
    void setProfile(const UserProfile& profile);

    [[nodiscard]] QLineEdit* displayNameEdit() const { return displayNameEdit_; }
    [[nodiscard]] QLineEdit* avatarUrlEdit() const { return avatarUrlEdit_; }
    [[nodiscard]] QLineEdit* emailEdit() const { return emailEdit_; }
    [[nodiscard]] QLineEdit* telegramChatIdEdit() const { return telegramChatIdEdit_; }
    [[nodiscard]] QPushButton* saveButton() const { return saveButton_; }
    [[nodiscard]] QLabel* statusLabel() const { return statusLabel_; }

signals:
    void saveRequested(const ProfileEdits& edits);

private:
    QLineEdit* displayNameEdit_ = nullptr;
    QLineEdit* avatarUrlEdit_ = nullptr;
    QLineEdit* emailEdit_ = nullptr;
    QLineEdit* telegramChatIdEdit_ = nullptr;
    QPushButton* saveButton_ = nullptr;
    QLabel* statusLabel_ = nullptr;
};

}  // namespace devicehub
