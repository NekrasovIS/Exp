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

private:
    QLabel* avatarLabel_ = nullptr;
    QLabel* profileLabel_ = nullptr;
    QPushButton* settingsButton_ = nullptr;
};

}  // namespace devicehub
