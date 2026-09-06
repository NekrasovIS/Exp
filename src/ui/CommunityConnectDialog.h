#pragma once

#include <QDialog>

class QLineEdit;
class QPushButton;

namespace devicehub {

/**
 * @brief Диалог "+" в CommunitiesPanel (issue #186) — присоединение к
 *        существующему сообществу по коду приглашения и создание нового
 *        в одном и том же окне, вместо прежнего одиночного QInputDialog
 *        только для создания и отдельного пункта "Join" в контекстном
 *        меню (требовавшего сначала видеть сообщество в общем списке
 *        всех существующих, который эта задача убирает).
 *
 * Чистое представление, как и остальные диалоги этого слоя (ModeratorsDialog,
 * ProfileDialog, SearchDialog) — только сигнализирует запросы, ничего не
 * знает про ChatRestClient; CommunitiesPanel решает, что делать дальше.
 */
class CommunityConnectDialog : public QDialog {
    Q_OBJECT

public:
    explicit CommunityConnectDialog(QWidget* parent = nullptr);

    [[nodiscard]] QLineEdit* inviteCodeEdit() const { return inviteCodeEdit_; }
    [[nodiscard]] QPushButton* joinButton() const { return joinButton_; }
    [[nodiscard]] QLineEdit* nameEdit() const { return nameEdit_; }
    [[nodiscard]] QPushButton* createButton() const { return createButton_; }

signals:
    /// Клик по "Join" с непустым кодом — сам диалог не проверяет, что
    /// код действительно существует (это решается на chat-service),
    /// только что поле не пустое.
    void joinRequested(const QString& code);
    /// Клик по "Create" с непустым именем.
    void createRequested(const QString& name);

private:
    void onJoinClicked();
    void onCreateClicked();

    QLineEdit* inviteCodeEdit_ = nullptr;
    QPushButton* joinButton_ = nullptr;
    QLineEdit* nameEdit_ = nullptr;
    QPushButton* createButton_ = nullptr;
};

}  // namespace devicehub
