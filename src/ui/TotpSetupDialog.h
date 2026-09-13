#pragma once

#include <QDialog>
#include <QStringList>

class QLabel;
class QLineEdit;
class QPushButton;
class QStackedWidget;

namespace devicehub {

struct TotpSetupInfo;

/**
 * @brief Диалог управления двухфакторной аутентификацией (TOTP, issue
 *        #388/#390) — открывается кнопкой в ProfileDialog. Три шага в
 *        QStackedWidget: статус (Enable, если TOTP выключена, иначе
 *        форма Disable с полем кода), настройка (QR-код + секрет +
 *        поле подтверждающего кода) и одноразовый показ backup-кодов.
 *
 * Чистое представление, тот же паттерн, что и у LoginWindow/
 * ProfileDialog — MainWindow владеет UserProfileClient и всей сетевой
 * логикой: enableRequested()/confirmRequested()/disableRequested()
 * сигналят наружу, showStatus()/showSetup()/showBackupCodes()/
 * showError() вызываются MainWindow по результатам
 * UserProfileClient::totpStatusReceived()/totpSetupStarted()/
 * totpConfirmed()/totpDisabled()/errorOccurred().
 */
class TotpSetupDialog : public QDialog {
    Q_OBJECT

public:
    explicit TotpSetupDialog(QWidget* parent = nullptr);

    /// Показывает страницу статуса — кнопку Enable, если @p enabled
    /// false, иначе форму Disable с полем кода. Также используется как
    /// "отмена" настройки/после успешного Disable — ничего не
    /// сохраняется на сервере до подтверждения, так что откат сюда
    /// всегда безопасен локально, без сетевого вызова.
    void showStatus(bool enabled);

    /// Показывает QR-код (сгенерированный из @p setup.otpauthUrl,
    /// QrCodeGenerator) и секрет текстом как запасной вариант ручного
    /// ввода, плюс поле подтверждающего кода.
    void showSetup(const TotpSetupInfo& setup);

    /// Показывает @p backupCodes один раз, с кнопкой "I've saved these"
    /// вместо автоматического возврата к статусу — пользователь должен
    /// явно подтвердить, что сохранил их.
    void showBackupCodes(const QStringList& backupCodes);

    /// Показывает сообщение об ошибке, не меняя текущую страницу.
    void showError(const QString& message);

    [[nodiscard]] QLabel* statusTextLabel() const { return statusTextLabel_; }
    [[nodiscard]] QPushButton* enableButton() const { return enableButton_; }
    [[nodiscard]] QLineEdit* disableCodeEdit() const { return disableCodeEdit_; }
    [[nodiscard]] QPushButton* disableButton() const { return disableButton_; }
    [[nodiscard]] QLabel* qrLabel() const { return qrLabel_; }
    [[nodiscard]] QLabel* secretLabel() const { return secretLabel_; }
    [[nodiscard]] QLineEdit* confirmCodeEdit() const { return confirmCodeEdit_; }
    [[nodiscard]] QPushButton* confirmButton() const { return confirmButton_; }
    [[nodiscard]] QPushButton* cancelSetupButton() const { return cancelSetupButton_; }
    [[nodiscard]] QLabel* backupCodesLabel() const { return backupCodesLabel_; }
    [[nodiscard]] QPushButton* backupCodesAcknowledgedButton() const { return backupCodesAcknowledgedButton_; }
    [[nodiscard]] QLabel* statusLabel() const { return statusLabel_; }

signals:
    /// "Enable" нажата на странице статуса.
    void enableRequested();

    /// "Confirm" нажата на странице настройки.
    void confirmRequested(const QString& code);

    /// "Disable" нажата на странице статуса (TOTP уже включена).
    void disableRequested(const QString& code);

private:
    void onConfirmClicked();
    void onCancelSetupClicked();
    void onDisableClicked();
    void onBackupCodesAcknowledgedClicked();

    QStackedWidget* stack_ = nullptr;
    QLabel* statusTextLabel_ = nullptr;
    QPushButton* enableButton_ = nullptr;
    QLineEdit* disableCodeEdit_ = nullptr;
    QPushButton* disableButton_ = nullptr;
    QLabel* qrLabel_ = nullptr;
    QLabel* secretLabel_ = nullptr;
    QLineEdit* confirmCodeEdit_ = nullptr;
    QPushButton* confirmButton_ = nullptr;
    QPushButton* cancelSetupButton_ = nullptr;
    QLabel* backupCodesLabel_ = nullptr;
    QPushButton* backupCodesAcknowledgedButton_ = nullptr;
    QLabel* statusLabel_ = nullptr;
};

}  // namespace devicehub
