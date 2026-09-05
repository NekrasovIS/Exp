#pragma once

#include <QWidget>

#include "chat/ChatRestClient.h"

class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;

namespace devicehub {

/**
 * @brief Основная область в режиме "Friends" (issue #187, Фаза 3) —
 *        замена ChatView для диалога личных сообщений: заголовок с
 *        логином собеседника, список сообщений, поле ввода + кнопка
 *        отправки (Enter тоже отправляет).
 *
 * Сильно упрощена по сравнению с ChatView: без вложений/поиска/
 * звонков/шифрования/редактирования — backend этой фазы (issue #187,
 * Фаза 2) их и не поддерживает для личных сообщений. Список сообщений
 * — обычные текстовые строки "login: текст", а не ChatBubble/
 * ChatMessageRow — тот же принцип "не гнаться за фичами, которых нет
 * на сервере", что и у самого backend'а этой фазы.
 *
 * Чистое представление — MainWindow владеет сетевым состоянием (какой
 * диалог сейчас открыт, поллинг новых сообщений) и вызывает
 * ChatRestClient сама.
 */
class DirectMessageView : public QWidget {
    Q_OBJECT

public:
    explicit DirectMessageView(QWidget* parent = nullptr);

    /// Показывает пустое состояние ("select a friend") — до того, как
    /// какой-либо диалог открыт.
    void showPlaceholder();

    /// Открывает диалог с @p otherLogin — очищает список сообщений
    /// (вызывающая сторона сама наполнит его через setMessages() после
    /// загрузки истории) и обновляет заголовок.
    void showThread(const QString& otherLogin);

    /// Заменяет список сообщений целиком (первичная загрузка истории).
    void setMessages(const QList<DirectMessageInfo>& messages);

    /// Добавляет одно новое сообщение в конец списка (поллинг новых
    /// сообщений, issue #187 — Фаза 2 backend'а пока не поддерживает
    /// живую доставку через WebSocket).
    void appendMessage(const DirectMessageInfo& message);

    [[nodiscard]] QListWidget* messagesList() const { return messagesList_; }
    [[nodiscard]] QLineEdit* messageEdit() const { return messageEdit_; }
    [[nodiscard]] QPushButton* sendButton() const { return sendButton_; }

signals:
    void sendMessageRequested(const QString& body);

private:
    void onSendClicked();

    QLabel* titleLabel_ = nullptr;
    QListWidget* messagesList_ = nullptr;
    QLineEdit* messageEdit_ = nullptr;
    QPushButton* sendButton_ = nullptr;
};

}  // namespace devicehub
