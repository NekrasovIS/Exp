#pragma once

#include <QRect>
#include <QWidget>

#include "user/UserProfileClient.h"

class QListWidget;
class QPoint;
class QPropertyAnimation;
class QPushButton;

namespace devicehub {

/**
 * @brief Секция боковой панели со списком друзей (issue #187, Фаза 3) —
 *        входящие заявки в друзья (принять/отклонить по правому клику)
 *        сверху, список друзей снизу (клик открывает диалог личных
 *        сообщений, правый клик — расфрендить), кнопка "+" отправляет
 *        новую заявку по логину.
 *
 * Открывается/сворачивается как всплывающая слева поверх своего
 * родителя панель (issue #216), а не заменяет ChannelsPanel в общей
 * раскладке — тот же приём "не участвует в layout'е родителя, сама
 * следит за его resize() через eventFilter", что уже использует
 * ToastBanner, только с анимацией слайда вместо мгновенного
 * появления/скрытия. Родитель должен быть виджетом, чей *собственный*
 * прямоугольник — это именно та область, которую нужно перекрыть при
 * открытии (в MainWindow это отдельный host-виджет ChannelsPanel, а не
 * весь sidebar — иначе панель перекрыла бы ещё и иконочную полосу
 * сообществ, которая должна оставаться видимой и кликабельной).
 *
 * Чистое представление, тот же паттерн, что и CommunitiesPanel/
 * ChannelsPanel — MainWindow наполняет её текущими данными, вызывает
 * setOpen()/isOpen() и реагирует на сигналы запросов, сама вызывая
 * UserProfileClient/ChatRestClient.
 */
class FriendsPanel : public QWidget {
    Q_OBJECT

public:
    explicit FriendsPanel(QWidget* parent = nullptr);

    /// Заменяет список друзей.
    void setFriends(const QStringList& logins);

    /// Заменяет список входящих заявок.
    void setIncomingRequests(const QList<FriendRequestInfo>& requests);

    /// Открывает (слайд слева направо, поверх родителя) или сворачивает
    /// (слайд обратно) панель — идемпотентно, повторный вызов с тем же
    /// @p open ничего не делает. MainWindow вызывает это из обработчика
    /// клика по кнопке "Friends" (переключатель) и при уходе из режима
    /// друзей любым другим путём (выбор сообщества, выход из аккаунта).
    void setOpen(bool open);
    [[nodiscard]] bool isOpen() const { return open_; }

    [[nodiscard]] QListWidget* friendsList() const { return friendsList_; }
    [[nodiscard]] QListWidget* requestsList() const { return requestsList_; }
    [[nodiscard]] QPushButton* addFriendButton() const { return addFriendButton_; }

signals:
    /// Клик по другу в списке — MainWindow открывает с ним диалог личных
    /// сообщений (openDmThreadWith()).
    void friendSelected(const QString& login);
    /// Клик по "+" после ввода логина в диалоге добавления — MainWindow
    /// отправляет заявку через UserProfileClient::sendFriendRequest().
    void addFriendRequested(const QString& recipientLogin);
    /// "Accept" в контекстном меню входящей заявки.
    void acceptRequestRequested(qint64 requestId);
    /// "Decline" в контекстном меню входящей заявки.
    void declineRequestRequested(qint64 requestId);
    /// "Remove Friend" в контекстном меню друга — расфрендить, работает
    /// в любую сторону пары (см. UserProfileClient::removeFriend()).
    void removeFriendRequested(const QString& login);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void showAddFriendDialog();
    void showFriendContextMenu(const QPoint& pos);
    void showRequestContextMenu(const QPoint& pos);
    [[nodiscard]] QRect visibleGeometry() const;
    [[nodiscard]] QRect hiddenGeometry() const;

    QListWidget* requestsList_ = nullptr;
    QListWidget* friendsList_ = nullptr;
    QPushButton* addFriendButton_ = nullptr;
    QPropertyAnimation* slideAnimation_ = nullptr;
    bool open_ = false;
};

}  // namespace devicehub
