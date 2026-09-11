#pragma once

#include <QWidget>

#include "chat/ChatRestClient.h"

class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QTimer;

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

    /// Добавляет одно новое сообщение в конец списка — issue #187,
    /// Фаза 2b: живая доставка через dmChatClient_ (WebSocket), не
    /// поллинг, несмотря на формулировку старого doc-комментария этого
    /// метода в предыдущих версиях.
    void appendMessage(const DirectMessageInfo& message);

    /// Показывает "<login> is typing…" на несколько секунд, затем
    /// автоматически скрывает — тот же паттерн, что и
    /// ChatView::showTypingUser() (issue #313: тот же кадр протокола,
    /// теперь и для диалогов).
    void showTypingUser(const QString& login);

    [[nodiscard]] QListWidget* messagesList() const { return messagesList_; }
    [[nodiscard]] QLineEdit* messageEdit() const { return messageEdit_; }
    [[nodiscard]] QPushButton* sendButton() const { return sendButton_; }
    [[nodiscard]] QLabel* typingIndicatorLabel() const { return typingIndicatorLabel_; }

signals:
    void sendMessageRequested(const QString& body);

    /// Пользователь печатает в поле сообщения — та же частотная
    /// throttle-логика, что и ChatView::typingRequested() (issue #313).
    void typingRequested();

private:
    void onSendClicked();

    QLabel* titleLabel_ = nullptr;
    QListWidget* messagesList_ = nullptr;
    QLineEdit* messageEdit_ = nullptr;
    QPushButton* sendButton_ = nullptr;
    QLabel* typingIndicatorLabel_ = nullptr;
    QTimer* typingIndicatorHideTimer_ = nullptr;
    QTimer* typingThrottleTimer_ = nullptr;
};

}  // namespace devicehub
