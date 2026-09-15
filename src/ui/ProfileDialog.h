#pragma once

#include <QDialog>

class QImage;
class QLabel;
class QLineEdit;
class QPushButton;

namespace devicehub {

struct UserProfile;
struct ProfileEdits;

/**
 * @brief Диалог редактирования профиля, открываемый пунктом "Edit
 *        Profile..." в меню по клику на аватар в футере (issue #110/
 *        #151): отображаемое имя, URL аватара, email (issue #156) и
 *        Telegram chat_id (issue #174) — любой из последних двух
 *        включает вход по одноразовому коду через этот канал.
 *
 * Чистое представление — MainWindow владеет UserProfileClient и всей
 * связующей логикой: он вызывает setProfile() для предзаполнения полей
 * (синхронизируется с каждым UserProfileClient::profileReceived()/
 * profileUpdated(), а не только при открытии) и слушает saveRequested(),
 * чтобы фактически отправить изменение — тот же паттерн "тупого
 * виджета", что и у SettingsDialog.
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

    /// Показывает настоящее изображение аватара в предпросмотре вместо
    /// буквы-заглушки (issue #384/#442) — вызывается MainWindow либо
    /// сразу после выбора файла (оптимистичный локальный предпросмотр),
    /// либо когда AvatarCache уже загрузила аватар для показанного
    /// профиля.
    void setAvatarImage(const QImage& image);

    [[nodiscard]] QLineEdit* displayNameEdit() const { return displayNameEdit_; }
    [[nodiscard]] QLineEdit* avatarUrlEdit() const { return avatarUrlEdit_; }
    [[nodiscard]] QLineEdit* emailEdit() const { return emailEdit_; }
    [[nodiscard]] QLineEdit* telegramChatIdEdit() const { return telegramChatIdEdit_; }
    [[nodiscard]] QPushButton* chooseAvatarFileButton() const { return chooseAvatarFileButton_; }
    [[nodiscard]] QLabel* avatarPreviewLabel() const { return avatarPreviewLabel_; }
    [[nodiscard]] QPushButton* saveButton() const { return saveButton_; }
    [[nodiscard]] QPushButton* cancelButton() const { return cancelButton_; }
    [[nodiscard]] QLabel* statusLabel() const { return statusLabel_; }

signals:
    void saveRequested(const ProfileEdits& edits);

    /// Клик по chooseAvatarFileButton() (issue #384/#442) — тот же
    /// паттерн, что и ChatView::attachFileRequested()/MainWindow::
    /// onAttachFileClicked(): сам диалог не открывает QFileDialog и не
    /// читает файл, только сигнализирует — MainWindow делает это и сам
    /// вызывает назад setAvatarImage() (оптимистичный предпросмотр) и
    /// UserProfileClient::uploadAvatar().
    void chooseAvatarFileRequested();

private:
    QLineEdit* displayNameEdit_ = nullptr;
    QLineEdit* avatarUrlEdit_ = nullptr;
    QLineEdit* emailEdit_ = nullptr;
    QLineEdit* telegramChatIdEdit_ = nullptr;
    QPushButton* chooseAvatarFileButton_ = nullptr;
    QLabel* avatarPreviewLabel_ = nullptr;
    QPushButton* saveButton_ = nullptr;
    QPushButton* cancelButton_ = nullptr;
    QLabel* statusLabel_ = nullptr;
};

}  // namespace devicehub
