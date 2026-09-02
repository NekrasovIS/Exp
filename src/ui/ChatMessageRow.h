#pragma once

#include <QString>
#include <QWidget>

#include <optional>

class QLabel;
class QResizeEvent;

namespace devicehub {

class ChatBubble;

/// Сообщение чата в том виде, в каком оно приходит из
/// ChatClient::messageReceived() — sentAt это исходная строка метки
/// времени сервера (сериализация Postgres, например
/// "2026-08-05 09:14:23.123456"). id и editedAt (issue #107) нужны,
/// чтобы адресовать/подписать конкретное сообщение для редактирования/
/// удаления; editedAt не задан для сообщения, которое никогда не
/// редактировалось. attachmentId равен -1, а attachmentFilename пуст,
/// когда у сообщения нет вложения (issue #116).
struct ChatMessage {
    qint64 id = 0;
    QString author;
    QString body;
    QString sentAt;
    std::optional<QString> editedAt;
    qint64 attachmentId = -1;
    QString attachmentFilename;
};

/**
 * @brief Одна строка в списке сообщений ChatView, оформленная в виде
 *        пузыря (в стиле iMessage/Slack).
 *
 * Собственные сообщения (@p isOwnMessage true) выравниваются по правому
 * краю в ChatBubble с зелёным градиентом, без аватара. Чужие сообщения
 * выравниваются по левому краю в нейтральном ChatBubble; полная форма
 * (@p showHeader true) добавляет кружок аватара плюс автора и время над
 * текстом, сгруппированная форма (для последовательного сообщения от
 * того же автора, см. chat_message_grouping::shouldGroupWithPrevious())
 * их опускает, чтобы последовательные сообщения не повторяли заголовок.
 *
 * Каждый размер здесь (диаметр аватара, отступы, padding/радиус пузыря)
 * выводится из метрик текущего шрифта, а не из фиксированной пиксельной
 * константы, а максимальная ширина пузыря пересчитывается как процент
 * от собственной ширины строки в resizeEvent(), а не задаётся жёстко —
 * так вся строка масштабируется вместе с размером шрифта и доступным
 * пространством, а не привязана к конкретным пиксельным числам.
 */
class ChatMessageRow : public QWidget {
    Q_OBJECT

public:
    ChatMessageRow(const ChatMessage& message, bool showHeader, bool isOwnMessage, QWidget* parent = nullptr);

    [[nodiscard]] qint64 messageId() const { return messageId_; }

    /// Обновляет отображаемый текст сообщения и добавляет метку
    /// "(edited)" рядом с меткой времени, если она у этой строки
    /// показана (showHeader) — issue #107, вызывается, когда
    /// ChatClient::messageEdited() срабатывает для сообщения этой строки.
    void updateBody(const QString& newBody);

signals:
    /// Выбор "Edit" в контекстном меню по правому клику (только для
    /// собственных сообщений, issue #107/#150) — @p currentBody
    /// позволяет вызывающему коду заранее заполнить поле редактирования,
    /// не разыскивая сообщение по id.
    void editRequested(qint64 id, const QString& currentBody);

    /// Выбор "Delete" в контекстном меню по правому клику (только для
    /// собственных сообщений, issue #150).
    void deleteRequested(qint64 id);

    /// Клик по "Download" на сообщении с вложением (issue #116, любое
    /// сообщение, не только собственное).
    void downloadRequested(qint64 attachmentId, const QString& filename);

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    ChatBubble* bubble_ = nullptr;
    QLabel* bodyLabel_ = nullptr;
    /// Null, если сконструировано с showHeader false — сгруппированные
    /// строки вообще не показывают метку времени, см. doc-комментарий
    /// класса.
    QLabel* timeLabel_ = nullptr;
    QString formattedSentAt_;
    qint64 messageId_ = 0;
};

}  // namespace devicehub
