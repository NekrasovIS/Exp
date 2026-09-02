#pragma once

#include <QDialog>

class QLabel;
class QLineEdit;
class QPushButton;
class QStackedWidget;

namespace devicehub {

/**
 * @brief Окно авторизации (issue #156) — единственная точка входа
 *        в приложение: показывается при запуске и блокирует доступ к
 *        интерфейсу, пока пользователь не авторизован (успешно вошёл
 *        или зарегистрировался) либо не закроет его, завершив
 *        приложение.
 *
 * Три шага в QStackedWidget: вход по одноразовому коду (ввод логина/
 * email/Telegram chat_id -> код отправлен, поле для его ввода) — шаги
 * 0/1, и вход по паролю/регистрация — шаг 2, доступный по ссылке "Войти
 * по паролю" с первого шага. Чистое представление, тот же паттерн, что
 * и у ProfileDialog/SettingsDialog — MainWindow владеет AuthClient и
 * всей сетевой логикой: requestCodeRequested()/verifyCodeRequested()/
 * passwordSignInRequested()/registerRequested() сигналят наружу,
 * showCodeSent()/showError() вызываются MainWindow по результатам
 * AuthClient::otpRequested()/tokenReceived()/errorOccurred().
 */
class LoginWindow : public QDialog {
    Q_OBJECT

public:
    explicit LoginWindow(QWidget* parent = nullptr);

    /// Переключает на второй шаг (ввод кода) — вызывается после
    /// успешного AuthClient::otpRequested(@p identifier).
    void showCodeSent(const QString& identifier);

    /// Показывает сообщение об ошибке (сетевая ошибка, неверный/
    /// просроченный код, занятый логин при регистрации и т.п.) — не
    /// меняет текущий шаг сам по себе.
    void showError(const QString& message);

    /// Возвращает окно к первому шагу с пустыми полями — вызывается
    /// перед повторным показом окна (например, после выхода из
    /// аккаунта).
    void reset();

    [[nodiscard]] QLineEdit* identifierEdit() const { return identifierEdit_; }
    [[nodiscard]] QPushButton* requestCodeButton() const { return requestCodeButton_; }
    [[nodiscard]] QPushButton* usePasswordButton() const { return usePasswordButton_; }
    [[nodiscard]] QLineEdit* codeEdit() const { return codeEdit_; }
    [[nodiscard]] QPushButton* verifyCodeButton() const { return verifyCodeButton_; }
    [[nodiscard]] QPushButton* backButton() const { return backButton_; }
    [[nodiscard]] QLineEdit* passwordLoginEdit() const { return passwordLoginEdit_; }
    [[nodiscard]] QLineEdit* passwordEdit() const { return passwordEdit_; }
    [[nodiscard]] QPushButton* passwordSignInButton() const { return passwordSignInButton_; }
    [[nodiscard]] QPushButton* registerButton() const { return registerButton_; }
    [[nodiscard]] QPushButton* backToCodeButton() const { return backToCodeButton_; }
    [[nodiscard]] QLabel* statusLabel() const { return statusLabel_; }

signals:
    /// "Send Code" нажата, @p identifier непустой (пустоту проверяем
    /// здесь же, в UI-слое — остальное проверяет ответ сервера).
    void requestCodeRequested(const QString& identifier);

    /// "Sign In" нажата на шаге ввода кода.
    void verifyCodeRequested(const QString& identifier, const QString& code);

    /// "Sign In" нажата на шаге пароля.
    void passwordSignInRequested(const QString& login, const QString& password);

    /// "Register" нажата на шаге пароля.
    void registerRequested(const QString& login, const QString& password);

private:
    void onRequestCodeClicked();
    void onVerifyCodeClicked();
    void onBackClicked();
    void onPasswordSignInClicked();
    void onRegisterClicked();

    QStackedWidget* stack_ = nullptr;
    QLineEdit* identifierEdit_ = nullptr;
    QPushButton* requestCodeButton_ = nullptr;
    QPushButton* usePasswordButton_ = nullptr;
    QLabel* codeSentLabel_ = nullptr;
    QLineEdit* codeEdit_ = nullptr;
    QPushButton* verifyCodeButton_ = nullptr;
    QPushButton* backButton_ = nullptr;
    QLineEdit* passwordLoginEdit_ = nullptr;
    QLineEdit* passwordEdit_ = nullptr;
    QPushButton* passwordSignInButton_ = nullptr;
    QPushButton* registerButton_ = nullptr;
    QPushButton* backToCodeButton_ = nullptr;
    QLabel* statusLabel_ = nullptr;
    /// Последний identifier, на который реально запрашивался код —
    /// verifyCodeRequested() передаёт именно его, а не текущий текст
    /// identifierEdit_ (пользователь мог не менять его, но на втором
    /// шаге поле уже не видно).
    QString pendingIdentifier_;
};

}  // namespace devicehub
