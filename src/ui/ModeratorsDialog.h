#pragma once

#include <QDialog>
#include <QStringList>

class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;

namespace devicehub {

/**
 * @brief Диалог управления модераторами, открываемый из пункта
 *        контекстного меню "Manage Moderators…" у CommunitiesPanel,
 *        доступного только владельцу (issue #114).
 *
 * Чистое представление — MainWindow владеет ChatRestClient и всей
 * связующей логикой, тот же паттерн "тупого виджета", что и у
 * ProfileDialog/SettingsDialog. setModerators() каждый раз заново
 * заполняет список с нуля (после того как каждое повышение/понижение
 * пройдёт через сервер и вернётся обратно), а не отслеживает состав
 * сам этот класс.
 */
class ModeratorsDialog : public QDialog {
    Q_OBJECT

public:
    explicit ModeratorsDialog(QWidget* parent = nullptr);

    /// Какое сообщество сейчас показывает этот диалог — MainWindow
    /// устанавливает это перед вызовом show() у диалога и считывает
    /// обратно при подключении promoteRequested()/demoteRequested() к
    /// ChatRestClient.
    void setCommunity(qint64 id, const QString& name);
    [[nodiscard]] qint64 communityId() const { return communityId_; }

    /// Заменяет содержимое списка модераторов.
    void setModerators(const QStringList& logins);

    [[nodiscard]] QListWidget* moderatorsList() const { return moderatorsList_; }
    [[nodiscard]] QLineEdit* loginEdit() const { return loginEdit_; }
    [[nodiscard]] QPushButton* promoteButton() const { return promoteButton_; }
    [[nodiscard]] QPushButton* demoteButton() const { return demoteButton_; }
    [[nodiscard]] QLabel* statusLabel() const { return statusLabel_; }

signals:
    /// @p communityId всегда равен communityId() — включён, чтобы
    /// вызывающему коду не приходилось отдельно отслеживать, для какого
    /// сообщества этот диалог открывался последний раз.
    void promoteRequested(qint64 communityId, const QString& login);
    void demoteRequested(qint64 communityId, const QString& login);

private:
    QListWidget* moderatorsList_ = nullptr;
    QLineEdit* loginEdit_ = nullptr;
    QPushButton* promoteButton_ = nullptr;
    QPushButton* demoteButton_ = nullptr;
    QLabel* statusLabel_ = nullptr;
    qint64 communityId_ = -1;
};

}  // namespace devicehub
