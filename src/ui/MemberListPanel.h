#pragma once

#include <QStringList>
#include <QWidget>

class QLabel;
class QListWidget;

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
 */
class MemberListPanel : public QWidget {
    Q_OBJECT

public:
    explicit MemberListPanel(QWidget* parent = nullptr);

    /// Заменяет список участников — сортирует по алфавиту сама, чтобы
    /// вызывающему коду не нужно было сортировать @p logins заранее.
    void setMembers(const QStringList& logins);

    [[nodiscard]] QLabel* titleLabel() const { return titleLabel_; }
    [[nodiscard]] QListWidget* listWidget() const { return listWidget_; }

private:
    QLabel* titleLabel_ = nullptr;
    QListWidget* listWidget_ = nullptr;
};

}  // namespace devicehub
