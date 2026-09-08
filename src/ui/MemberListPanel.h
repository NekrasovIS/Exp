#pragma once

#include <QStringList>
#include <QWidget>

class QLabel;
class QListWidget;
class QPoint;

namespace devicehub {

/**
 * @brief Список участников выбранного сообщества, справа от ChatView —
 *        новый элемент раскладки (issue #182), которого раньше в
 *        DeviceHub не было вовсе.
 *
 * Чистое представление — MainWindow вызывает setMembers() всякий раз,
 * как приходит ChatRestClient::membersListed() для текущего выбранного
 * сообщества (тот же REST-вызов listMembers(), что уже использовался
 * для заворачивания ключей зашифрованных каналов, issue #138 — теперь
 * ещё и для этой панели).
 *
 * Контекстное меню участника (issue #217) — тот же паттерн
 * customContextMenuRequested()/QMenu, что уже использует
 * CommunitiesPanel — предлагает "Grant channel key access", пока
 * setChannelEncrypted(true) (открытый сейчас канал зашифрован) и строка
 * не совпадает с setCurrentUserLogin() (себе самому доступ не выдают).
 * Сама панель не проверяет, есть ли у вошедшего пользователя
 * собственный развёрнутый ключ этого канала — MainWindow решает это при
 * обработке grantChannelKeyAccessRequested() и показывает явную ошибку,
 * если ключа нет.
 */
class MemberListPanel : public QWidget {
    Q_OBJECT

public:
    explicit MemberListPanel(QWidget* parent = nullptr);

    /// Заменяет список участников — сортирует по алфавиту сама, чтобы
    /// вызывающему коду не нужно было сортировать @p logins заранее.
    void setMembers(const QStringList& logins);

    /// Нужен, чтобы не предлагать "Grant channel key access" для
    /// собственной строки в списке.
    void setCurrentUserLogin(const QString& login);
    /// Управляет тем, показывать ли вообще "Grant channel key access" в
    /// контекстном меню — есть смысл только пока открытый в ChatView
    /// канал зашифрован (issue #217).
    void setChannelEncrypted(bool encrypted);

    [[nodiscard]] QLabel* titleLabel() const { return titleLabel_; }
    [[nodiscard]] QListWidget* listWidget() const { return listWidget_; }

signals:
    /// Клик по "Grant channel key access" для @p login в контекстном
    /// меню (issue #217) — MainWindow оборачивает уже развёрнутый у
    /// вошедшего пользователя ключ ТЕКУЩЕГО открытого канала под
    /// публичный ключ @p login и публикует через ChatRestClient::
    /// setChannelKey(); показывает явную ошибку, если у вошедшего
    /// пользователя самого нет ключа этого канала, или у @p login ещё
    /// нет опубликованного открытого ключа.
    void grantChannelKeyAccessRequested(const QString& login);

private:
    void showContextMenu(const QPoint& pos);

    QLabel* titleLabel_ = nullptr;
    QListWidget* listWidget_ = nullptr;
    QString currentUserLogin_;
    bool channelEncrypted_ = false;
};

}  // namespace devicehub
