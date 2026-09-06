#pragma once

#include <QWidget>

#include "user/UserProfileClient.h"

class QListWidget;
class QPoint;
class QPushButton;

namespace devicehub {

/**
 * @brief Секция боковой панели, заменяющая ChannelsPanel в режиме
 *        "Friends" (issue #187, Фаза 3) — входящие заявки в друзья
 *        (принять/отклонить по правому клику) сверху, список друзей
 *        снизу (клик открывает диалог личных сообщений, правый клик —
 *        расфрендить), кнопка "+" отправляет новую заявку по логину.
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

signals:
    void friendSelected(const QString& login);
    void addFriendRequested(const QString& recipientLogin);
    void acceptRequestRequested(qint64 requestId);
    void declineRequestRequested(qint64 requestId);
    void removeFriendRequested(const QString& login);

private:
    void showAddFriendDialog();
    void showFriendContextMenu(const QPoint& pos);
    void showRequestContextMenu(const QPoint& pos);

    QListWidget* requestsList_ = nullptr;
    QListWidget* friendsList_ = nullptr;
    QPushButton* addFriendButton_ = nullptr;
};

}  // namespace devicehub
