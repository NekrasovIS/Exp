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
/// когда у сообщения нет вложения (issue #116). isPinned (issue #338)
/// отражает состояние на момент загрузки/получения — ChatMessageRow
/// сама не резолвит его, только рисует то, что передано.
struct ChatMessage {
    qint64 id = 0;
    QString author;
    QString body;
    QString sentAt;
    std::optional<QString> editedAt;
    qint64 attachmentId = -1;
    QString attachmentFilename;
    bool isPinned = false;
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
    /// @p canManageChannel (issue #338) — владелец канала/сообщества
    /// или модератор сообщества, владеющего каналом, куда идёт эта
    /// строка; управляет только видимостью Pin/Unpin в контекстном
    /// меню, не связано с @p isOwnMessage (закреплять/снимать может не
    /// автор, а тот, у кого есть эта роль — см. doc-комментарий класса
    /// ChatClient::sendPinMessage()).
    ChatMessageRow(const ChatMessage& message, bool showHeader, bool isOwnMessage, bool canManageChannel = false,
                   QWidget* parent = nullptr);

    [[nodiscard]] qint64 messageId() const { return messageId_; }

    /// Обновляет отображаемый текст сообщения и добавляет метку
    /// "(edited)" рядом с меткой времени, если она у этой строки
    /// показана (showHeader) — issue #107, вызывается, когда
    /// ChatClient::messageEdited() срабатывает для сообщения этой строки.
    void updateBody(const QString& newBody);

    /// Обновляет закреплённость на месте (issue #338) — вызывается,
    /// когда ChatClient::messagePinned()/messageUnpinned() срабатывает
    /// для сообщения этой строки; переключает видимость значка
    /// "📌 Pinned" и текст пункта меню Pin/Unpin при следующем открытии
    /// контекстного меню.
    void setPinned(bool isPinned);

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

    /// Выбор "Pin" в контекстном меню (issue #338, только когда
    /// сконструировано с canManageChannel true).
    void pinRequested(qint64 id);

    /// Выбор "Unpin" — то же условие видимости, что и у pinRequested().
    void unpinRequested(qint64 id);

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
    /// Плейсхолдер превью изображения-вложения (issue #188) — null, если
    /// у сообщения нет вложения-изображения. setAttachmentPreview()
    /// заменяет плейсхолдерный текст на реальную картинку, когда она
    /// загружена.
    QLabel* attachmentPreviewLabel_ = nullptr;
    /// Issue #338 — управляет видимостью Pin/Unpin в контекстном меню,
    /// построенном лениво при каждом правом клике (см. конструктор), а
    /// не пересобираемом при setPinned().
    bool canManageChannel_ = false;
    bool isPinned_ = false;
    /// Виден только пока isPinned_ true — null до первого раза, когда
    /// это стало нужным, создаётся лениво в setPinned() ИЛИ в
    /// конструкторе, если сообщение изначально уже закреплено.
    QLabel* pinnedIndicatorLabel_ = nullptr;
};

}  // namespace devicehub
