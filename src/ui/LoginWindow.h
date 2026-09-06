#pragma once

#include <QDialog>

class QLabel;
class QLineEdit;
class QPushButton;
class QStackedWidget;

namespace devicehub {

/**
 * @brief Окно входа по одноразовому коду (issue #156) — показывается
 *        при запуске, пока пользователь не авторизован.
 *
 * Два шага в QStackedWidget: ввод логина/email -> код отправлен, поле
 * для его ввода. Чистое представление, тот же паттерн, что и у
 * ProfileDialog/SettingsDialog — MainWindow владеет AuthClient и всей
 * сетевой логикой: requestCodeRequested()/verifyCodeRequested() сигналят
 * наружу, showCodeSent()/showError() вызываются MainWindow по
 * результатам AuthClient::otpRequested()/tokenReceived()/
 * errorOccurred().
 *
 * Логин по паролю в AccountMenu (правый верхний угол) продолжает
 * работать как раньше и тоже закрывает это окно при успехе — вход по
 * коду не заменяет его, а добавляется как основной способ входа.
 */
class LoginWindow : public QDialog {
    Q_OBJECT

public:
    explicit LoginWindow(QWidget* parent = nullptr);

    /// Переключает на второй шаг (ввод кода) — вызывается после
    /// успешного AuthClient::otpRequested(@p identifier).
    void showCodeSent(const QString& identifier);

    /// Показывает сообщение об ошибке (сетевая ошибка, неверный/
    /// просроченный код и т.п.) — не меняет текущий шаг сам по себе.
    void showError(const QString& message);

    /// Возвращает окно к первому шагу с пустыми полями — вызывается
    /// перед повторным показом окна (например, после выхода из
    /// аккаунта).
    void reset();

    [[nodiscard]] QLineEdit* identifierEdit() const { return identifierEdit_; }
    [[nodiscard]] QPushButton* requestCodeButton() const { return requestCodeButton_; }
    [[nodiscard]] QLineEdit* codeEdit() const { return codeEdit_; }
    [[nodiscard]] QPushButton* verifyCodeButton() const { return verifyCodeButton_; }
    [[nodiscard]] QPushButton* backButton() const { return backButton_; }
    [[nodiscard]] QLabel* statusLabel() const { return statusLabel_; }

signals:
    /// "Send Code" нажата, @p identifier непустой (пустоту проверяем
    /// здесь же, в UI-слое — остальное проверяет ответ сервера).
    void requestCodeRequested(const QString& identifier);

    /// "Sign In" нажата на втором шаге.
    void verifyCodeRequested(const QString& identifier, const QString& code);

private:
    void onRequestCodeClicked();
    void onVerifyCodeClicked();
    void onBackClicked();

    QStackedWidget* stack_ = nullptr;
    QLineEdit* identifierEdit_ = nullptr;
    QPushButton* requestCodeButton_ = nullptr;
    QLabel* codeSentLabel_ = nullptr;
    QLineEdit* codeEdit_ = nullptr;
    QPushButton* verifyCodeButton_ = nullptr;
    QPushButton* backButton_ = nullptr;
    QLabel* statusLabel_ = nullptr;
    /// Последний identifier, на который реально запрашивался код —
    /// verifyCodeRequested() передаёт именно его, а не текущий текст
    /// identifierEdit_ (пользователь мог не менять его, но на втором
    /// шаге поле уже не видно).
    QString pendingIdentifier_;
};

}  // namespace devicehub
