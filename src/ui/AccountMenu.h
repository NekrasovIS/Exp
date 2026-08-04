#pragma once

#include <QWidget>

class QFrame;
class QLabel;
class QLineEdit;
class QPushButton;

namespace devicehub {

/**
 * @brief Top-right account control: a button that toggles a popup with
 *        login/password fields, a "get token" action, and auth status.
 *
 * Pure presentation (the same controls the old Authorization tab exposed,
 * just relocated) — MainWindow still owns AuthClient and all the wiring.
 */
class AccountMenu : public QWidget {
    Q_OBJECT

public:
    explicit AccountMenu(QWidget* parent = nullptr);

    [[nodiscard]] QLineEdit* loginEdit() const { return loginEdit_; }
    [[nodiscard]] QLineEdit* passwordEdit() const { return passwordEdit_; }
    [[nodiscard]] QPushButton* requestTokenButton() const { return requestTokenButton_; }
    [[nodiscard]] QLabel* statusLabel() const { return statusLabel_; }
    [[nodiscard]] QPushButton* toggleButton() const { return toggleButton_; }

private:
    void togglePopup();

    QPushButton* toggleButton_ = nullptr;
    QFrame* popup_ = nullptr;
    QLineEdit* loginEdit_ = nullptr;
    QLineEdit* passwordEdit_ = nullptr;
    QPushButton* requestTokenButton_ = nullptr;
    QLabel* statusLabel_ = nullptr;
};

}  // namespace devicehub
