#pragma once

#include <QWidget>

#include "user/UserProfileClient.h"

class QListWidget;
class QPoint;
class QPushButton;

namespace devicehub {

/**
 * @brief Секция боковой панели, заменяющая ChannelsPanel в режиме
 *        "Friends" (issue #187, Фаза 3) — кнопка "< Communities" сверху
 *        слева переключает обратно в режим сообществ, входящие заявки
 *        в друзья (принять/отклонить по правому клику) сверху, список
 *        друзей снизу (клик открывает диалог личных сообщений, правый
 *        клик — расфрендить), кнопка "+" отправляет новую заявку по
 *        логину.
 *
 * Чистое представление, тот же паттерн, что и CommunitiesPanel/
 * ChannelsPanel — MainWindow наполняет её текущими данными и реагирует
 * на сигналы запросов, сама вызывая UserProfileClient/ChatRestClient.
 */
class FriendsPanel : public QWidget {
    Q_OBJECT

public:
    explicit FriendsPanel(QWidget* parent = nullptr);

    /// Заменяет список друзей.
    void setFriends(const QStringList& logins);

    /// Заменяет список входящих заявок.
    void setIncomingRequests(const QList<FriendRequestInfo>& requests);

    [[nodiscard]] QListWidget* friendsList() const { return friendsList_; }
    [[nodiscard]] QListWidget* requestsList() const { return requestsList_; }
    [[nodiscard]] QPushButton* addFriendButton() const { return addFriendButton_; }
    [[nodiscard]] QPushButton* backButton() const { return backButton_; }

signals:
    /// Клик по "< Communities" — MainWindow переключает обратно в режим
    /// сообществ (showCommunitiesMode()). Единственный путь назад: сама
    /// CommunitiesPanel (с её кнопкой "Friends") при входе в этот режим
    /// заменяется этой панелью в боковой раскладке и больше не видна.
    void backToCommunitiesRequested();
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

private:
    void showAddFriendDialog();
    void showFriendContextMenu(const QPoint& pos);
    void showRequestContextMenu(const QPoint& pos);

    QListWidget* requestsList_ = nullptr;
    QListWidget* friendsList_ = nullptr;
    QPushButton* addFriendButton_ = nullptr;
    QPushButton* backButton_ = nullptr;
};

}  // namespace devicehub
