#pragma once

#include <QWidget>

#include "chat/ChatRestClient.h"

class QListWidget;
class QPoint;
class QPushButton;

namespace devicehub {

class CommunityConnectDialog;

/**
 * @brief Узкая иконочная полоса в самом левом краю боковой панели:
 *        по одному значку-аватару на сообщество (первая буква, зелёный
 *        градиент), кнопка обновления сверху и кнопка "+" внизу,
 *        открывающая CommunityConnectDialog (присоединение по коду
 *        приглашения или создание нового, issue #186); правый клик —
 *        для переименования/удаления/копирования кода приглашения.
 *
 * Список (issue #186) — только сообщества, в которых уже состоит
 * вошедший пользователь, а не все существующие: подключиться к новому
 * теперь можно только по коду приглашения, не выбором из общего
 * списка — отсюда и переход от "Join" в контекстном меню (нужен был
 * общий список, чтобы вообще увидеть, что присоединять) к
 * CommunityConnectDialog.
 *
 * Чистое представление — не владеет никаким сетевым состоянием.
 * MainWindow передаёт ей текущий список сообществ и логин вошедшего
 * пользователя (чтобы решить, предлагать ли переименование/удаление
 * для конкретного элемента — на сервере это тоже разрешено только
 * владельцу, здесь это просто избавляет от показа действий, которые
 * всё равно вернут только 403) и реагирует на сигналы запросов, сама
 * вызывая ChatRestClient.
 */
class CommunitiesPanel : public QWidget {
    Q_OBJECT

public:
    explicit CommunitiesPanel(QWidget* parent = nullptr);

    /// Заменяет содержимое списка.
    void setCommunities(const QList<ChatItem>& communities);

    /// Выбирает элемент с @p id, если он есть, не порождая при этом
    /// communitySelected() — используется, чтобы отразить выбор,
    /// который MainWindow уже сделал сам (например, автовыбор только
    /// что созданного сообщества), не вызывая повторно тот же запрос.
    void selectCommunityId(qint64 id);

    /// Нужно, чтобы решить, предлагать ли переименование/удаление
    /// для элемента.
    void setCurrentUserLogin(const QString& login);

    [[nodiscard]] QListWidget* listWidget() const { return listWidget_; }
    [[nodiscard]] QPushButton* addButton() const { return addButton_; }
    [[nodiscard]] QPushButton* refreshButton() const { return refreshButton_; }
    [[nodiscard]] CommunityConnectDialog* connectDialog() const { return connectDialog_; }

signals:
    void createRequested(const QString& name);
    void renameRequested(qint64 id, const QString& newName);
    void deleteRequested(qint64 id);
    /// Клик по "Join" в CommunityConnectDialog (issue #186) — заменяет
    /// прежний joinRequested(qint64 id): до ответа сервера вызывающая
    /// сторона знает только код, не id сообщества.
    void joinByCodeRequested(const QString& code);
    /// Клик по "Regenerate Invite Code" в контекстном меню (issue #186,
    /// только для владельца — прежний код @p id сразу перестаёт
    /// работать).
    void regenerateInviteCodeRequested(qint64 id);
    /// Клик по "Manage Moderators…" (только для владельца, issue #114)
    /// — @p name позволяет MainWindow озаглавить диалог без отдельного
    /// поиска.
    void manageModeratorsRequested(qint64 id, const QString& name);
    void communitySelected(qint64 id);

private:
    void showAddDialog();
    void showContextMenu(const QPoint& pos);

    QListWidget* listWidget_ = nullptr;
    QPushButton* addButton_ = nullptr;
    QPushButton* refreshButton_ = nullptr;
    CommunityConnectDialog* connectDialog_ = nullptr;
    QString currentUserLogin_;
};

}  // namespace devicehub
