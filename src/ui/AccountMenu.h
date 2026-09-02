#pragma once

#include <QWidget>

class QFrame;
class QLabel;
class QLineEdit;
class QPushButton;

namespace devicehub {

/**
 * @brief Элемент управления аккаунтом в правом верхнем углу: кнопка,
 *        открывающая всплывающее окно с полями логина/пароля, действием
 *        "получить токен" и статусом авторизации.
 *
 * Чистое представление (те же элементы управления, что были на старой
 * вкладке Authorization, просто перенесённые) — MainWindow по-прежнему
 * владеет AuthClient и всей связующей логикой.
 */
class AccountMenu : public QWidget {
    Q_OBJECT

public:
    explicit AccountMenu(QWidget* parent = nullptr);

    [[nodiscard]] QLineEdit* loginEdit() const { return loginEdit_; }
    [[nodiscard]] QLineEdit* passwordEdit() const { return passwordEdit_; }
    [[nodiscard]] QPushButton* requestTokenButton() const { return requestTokenButton_; }
    [[nodiscard]] QPushButton* registerButton() const { return registerButton_; }
    [[nodiscard]] QPushButton* editProfileButton() const { return editProfileButton_; }
    [[nodiscard]] QLabel* statusLabel() const { return statusLabel_; }
    [[nodiscard]] QPushButton* toggleButton() const { return toggleButton_; }

    /// Имеет смысл только при выполненном входе — MainWindow включает её,
    /// как только AuthClient::tokenVerified() сообщает о валидном токене,
    /// и отключает в противном случае (issue #110).
    void setEditProfileEnabled(bool enabled);

private:
    void togglePopup();

    QPushButton* toggleButton_ = nullptr;
    QFrame* popup_ = nullptr;
    QLineEdit* loginEdit_ = nullptr;
    QLineEdit* passwordEdit_ = nullptr;
    QPushButton* requestTokenButton_ = nullptr;
    QPushButton* registerButton_ = nullptr;
    QPushButton* editProfileButton_ = nullptr;
    QLabel* statusLabel_ = nullptr;
};

}  // namespace devicehub
