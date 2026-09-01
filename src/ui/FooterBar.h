#pragma once

#include <QWidget>

class QLabel;
class QPushButton;

namespace devicehub {

/**
 * @brief Bottom bar: the current user's mini-profile on the left plus a
 *        gear button that opens SettingsDialog.
 */
class FooterBar : public QWidget {
    Q_OBJECT

public:
    explicit FooterBar(QWidget* parent = nullptr);

    /// Updates the profile text (e.g. the signed-in login, or a
    /// not-signed-in placeholder).
    void setProfileText(const QString& text);

    [[nodiscard]] QPushButton* settingsButton() const { return settingsButton_; }
    [[nodiscard]] QLabel* avatarLabel() const { return avatarLabel_; }

signals:
    /// Emitted when the avatar is clicked (issue #151) — MainWindow
    /// shows the account menu (edit profile / sign out) from here
    /// instead of the caller having to reach into avatarLabel() and
    /// wire up its own event filter.
    void accountSettingsRequested();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    QLabel* avatarLabel_ = nullptr;
    QLabel* profileLabel_ = nullptr;
    QPushButton* settingsButton_ = nullptr;
};

}  // namespace devicehub
