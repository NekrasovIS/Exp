#pragma once

#include <QString>
#include <QWidget>

#include <optional>

class QImage;
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
///
/// replyToMessageId равен -1, когда это не ответ (issue #306) — тот же
/// стиль сентинела, что и у attachmentId. Когда >= 0, replyToAuthor/
/// replyToBodySnippet заполняются ChatView из собственного кэша уже
/// показанных сообщений (messagesById_) ПЕРЕД конструированием строки —
/// сама эта структура и ChatMessageRow ничего не резолвят самостоятельно.
/// Оба остаются пустыми, если ChatView не нашла это id в своём кэше
/// (сообщение удалено, либо старше уже подгруженного окна истории) —
/// ChatMessageRow показывает это как "message unavailable", отличая
/// такой случай от "это не ответ вообще" по одному только
/// replyToMessageId.
struct ChatMessage {
    qint64 id = 0;
    QString author;
    QString body;
    QString sentAt;
    std::optional<QString> editedAt;
    qint64 attachmentId = -1;
    QString attachmentFilename;
    qint64 replyToMessageId = -1;
    QString replyToAuthor;
    QString replyToBodySnippet;
};

/// True, если @p filename оканчивается на одно из известных расширений
/// растровых изображений (issue #188) — ChatView запрашивает превью
/// только для таких вложений, не для произвольных файлов. Регистр
/// расширения не важен.
[[nodiscard]] bool isImageAttachment(const QString& filename);

/// То же самое, для видео (issue #188) — только чтобы показать значок-
/// заглушку вместо кнопки "Download" в бабле, без реальной загрузки
/// файла (в отличие от изображений, у видео нет дешёвого способа
/// получить превью-кадр без скачивания и декодирования всего файла).
[[nodiscard]] bool isVideoAttachment(const QString& filename);

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

    /// Заменяет плейсхолдер превью изображения-вложения на реально
    /// загруженный @p image (issue #188), масштабируя с сохранением
    /// пропорций под текущую максимальную ширину плитки превью. Ничего
    /// не делает, если у этой строки нет вложения-изображения —
    /// ChatView сам решает, кому из строк это вообще может пригодиться
    /// (see isImageAttachment()), но передаёт данные сюда без повторной
    /// проверки, поэтому вызывающий код может звать это безусловно.
    void setAttachmentPreview(const QImage& image);

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

    /// Выбор "Reply" в контекстном меню по правому клику (issue #306) —
    /// доступно на любом сообщении, не только собственном, в отличие от
    /// editRequested/deleteRequested. Слушатель (ChatView) сам решает,
    /// что показать как "цитата" в поле ввода — эта строка передаёт
    /// только id.
    void replyRequested(qint64 id);

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
    /// Тело сообщения ДО message_formatting::highlightMentions() (issue
    /// #307) — editRequested() эмиттит это, а не bodyLabel_->text(),
    /// иначе поле редактирования предзаполнилось бы уже обёрнутым в
    /// **bold** markdown текстом, и повторное сохранение без изменений
    /// зафиксировало бы эту обёртку в самом сообщении навсегда.
    QString rawBody_;
    /// Плейсхолдер превью изображения-вложения (issue #188) — null, если
    /// у сообщения нет вложения-изображения. setAttachmentPreview()
    /// заменяет плейсхолдерный текст на реальную картинку, когда она
    /// загружена.
    QLabel* attachmentPreviewLabel_ = nullptr;
};

}  // namespace devicehub
