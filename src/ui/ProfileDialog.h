#pragma once

#include <QDialog>

class QLabel;
class QLineEdit;
class QPushButton;

namespace devicehub {

struct UserProfile;
struct ProfileEdits;

/**
 * @brief Диалог редактирования профиля, открываемый кнопкой "Edit
 *        Profile" у AccountMenu (issue #110): отображаемое имя, URL
 *        аватара и email (issue #156 — нужен, чтобы работал вход по
 *        одноразовому коду).
 *
 * Чистое представление — MainWindow владеет UserProfileClient и всей
 * связующей логикой: он вызывает setProfile() для предзаполнения полей
 * (синхронизируется с каждым UserProfileClient::profileReceived()/
 * profileUpdated(), а не только при открытии) и слушает saveRequested(),
 * чтобы фактически отправить изменение — тот же паттерн "тупого
 * виджета", что и у SettingsDialog/AccountMenu.
 */
class ProfileDialog : public QDialog {
    Q_OBJECT

public:
    explicit ProfileDialog(QWidget* parent = nullptr);

    /// Предзаполняет поля — ничего не делает с полем, находящимся сейчас
    /// в фокусе, поэтому безопасно вызывать, пока пользователь
    /// редактирует (например, фоновое обновление приходит, пока диалог
    /// как раз открыт).
    void setProfile(const UserProfile& profile);

    [[nodiscard]] QLineEdit* displayNameEdit() const { return displayNameEdit_; }
    [[nodiscard]] QLineEdit* avatarUrlEdit() const { return avatarUrlEdit_; }
    [[nodiscard]] QLineEdit* emailEdit() const { return emailEdit_; }
    [[nodiscard]] QPushButton* saveButton() const { return saveButton_; }
    [[nodiscard]] QLabel* statusLabel() const { return statusLabel_; }

signals:
    void saveRequested(const ProfileEdits& edits);

private:
    QLineEdit* displayNameEdit_ = nullptr;
    QLineEdit* avatarUrlEdit_ = nullptr;
    QLineEdit* emailEdit_ = nullptr;
    QPushButton* saveButton_ = nullptr;
    QLabel* statusLabel_ = nullptr;
};

}  // namespace devicehub
