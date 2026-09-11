#pragma once

#include <QSet>
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
    /// Presence-состояние (issue #309), если уже известно, переживает
    /// это — рисуется по уже накопленному onlineLogins_, не сбрасывается
    /// каждым обновлением списка через REST.
    void setMembers(const QStringList& logins);

    /// Начальный снимок присутствия (issue #309) — ChatClient::
    /// onlineMembersReceived() при подписке на канал сообщества;
    /// заменяет весь набор целиком и перерисовывает уже показанные
    /// строки.
    void setOnlineLogins(const QStringList& logins);

    /// Живое обновление присутствия одного участника (issue #309) —
    /// ChatClient::presenceChanged(); обновляет иконку строки на месте,
    /// без пересборки всего списка, если @p login сейчас показан.
    void setLoginOnline(const QString& login, bool online);

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
    /// Presence (issue #309) — survives setMembers() rebuilding the
    /// list (a REST refresh shouldn't blank out live presence state).
    QSet<QString> onlineLogins_;
};

}  // namespace devicehub
