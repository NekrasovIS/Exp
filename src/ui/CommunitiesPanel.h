#pragma once

#include <QWidget>

#include "chat/ChatRestClient.h"

class QListWidget;
class QPoint;
class QPushButton;

namespace devicehub {

/**
 * @brief Узкая иконочная полоса в самом левом краю боковой панели:
 *        кнопка "Friends" (issue #187) сверху, по одному значку-аватару
 *        на сообщество (первая буква, зелёный градиент), кнопка
 *        обновления и кнопка "+" внизу для создания нового; правый клик
 *        — для присоединения/переименования/удаления.
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
    [[nodiscard]] QPushButton* friendsButton() const { return friendsButton_; }

signals:
    void createRequested(const QString& name);
    void renameRequested(qint64 id, const QString& newName);
    void deleteRequested(qint64 id);
    void joinRequested(qint64 id);
    /// Клик по "Manage Moderators…" (только для владельца, issue #114)
    /// — @p name позволяет MainWindow озаглавить диалог без отдельного
    /// поиска.
    void manageModeratorsRequested(qint64 id, const QString& name);
    void communitySelected(qint64 id);
    /// Клик по кнопке "Friends" (issue #187) — переключает MainWindow в
    /// режим друзей/личных сообщений вместо сообществ/каналов.
    void friendsRequested();

private:
    void showAddDialog();
    void showContextMenu(const QPoint& pos);

    QListWidget* listWidget_ = nullptr;
    QPushButton* addButton_ = nullptr;
    QPushButton* refreshButton_ = nullptr;
    QPushButton* friendsButton_ = nullptr;
    QString currentUserLogin_;
};

}  // namespace devicehub
