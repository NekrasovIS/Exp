#pragma once

#include <QList>
#include <QString>
#include <QStringList>
#include <QWidget>

#include <optional>

class QImage;
class QLabel;
class QResizeEvent;

namespace devicehub {

class ChatBubble;

/// Одна агрегированная реакция на сообщение, для отображения (issue
/// #334) — та же форма, что и MessageReaction на стороне chat-service
/// (см. WebSocketServer::toJson(), поле "reactions" у сообщения),
/// просто в Qt-типах. @p logins — все, кто поставил именно эту эмодзи,
/// в порядке, в котором это отдал сервер (кто поставил раньше — раньше
/// в списке).
struct MessageReactionSummary {
    QString emoji;
    QStringList logins;
};

/// Сообщение чата в том виде, в каком оно приходит из
/// ChatClient::messageReceived() — sentAt это исходная строка метки
/// времени сервера (сериализация Postgres, например
/// "2026-08-05 09:14:23.123456"). id и editedAt (issue #107) нужны,
/// чтобы адресовать/подписать конкретное сообщение для редактирования/
/// удаления; editedAt не задан для сообщения, которое никогда не
/// редактировалось. attachmentId равен -1, а attachmentFilename пуст,
/// когда у сообщения нет вложения (issue #116). reactions пуст для
/// сообщения, на которое пока никто не поставил реакцию (issue #334).
struct ChatMessage {
    qint64 id = 0;
    QString author;
    QString body;
    QString sentAt;
    std::optional<QString> editedAt;
    qint64 attachmentId = -1;
    QString attachmentFilename;
    QList<MessageReactionSummary> reactions;
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
    /// @p currentUserLogin (issue #334) — нужен только чтобы решить,
    /// какая из чипов-реакций под баблом — "моя" (выделяется отдельным
    /// стилем); пустая строка (значение по умолчанию) — ни одна чужая
    /// реакция никогда не совпадёт с пустым логином, так что это
    /// безопасный сентинел "текущий пользователь неизвестен", а не
    /// отдельный bool-флаг.
    ChatMessageRow(const ChatMessage& message, bool showHeader, bool isOwnMessage,
                   const QString& currentUserLogin = QString(), QWidget* parent = nullptr);

    [[nodiscard]] qint64 messageId() const { return messageId_; }

    /// Обновляет отображаемый текст сообщения и добавляет метку
    /// "(edited)" рядом с меткой времени, если она у этой строки
    /// показана (showHeader) — issue #107, вызывается, когда
    /// ChatClient::messageEdited() срабатывает для сообщения этой строки.
    void updateBody(const QString& newBody);

    /// Применяет одно изменение реакции (issue #334) — @p logins это
    /// ПОЛНЫЙ список тех, кто сейчас поставил именно @p emoji на это
    /// сообщение (как приходит в ChatClient::reactionChanged(), см. её
    /// doc-комментарий), не дельта. Пустой @p logins убирает чип этой
    /// эмодзи целиком, а не показывает "0". Другие эмодзи на этом же
    /// сообщении не трогает.
    void applyReactionChange(const QString& emoji, const QStringList& logins);

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

    /// Выбор эмодзи в подменю "React" контекстного меню, либо клик по
    /// уже существующему чипу-реакции под баблом (issue #334) — оба
    /// пути ведут к одному и тому же переключению (toggle) на стороне
    /// сервера, поэтому оба эмитят один и тот же сигнал. Доступно на
    /// любом сообщении, не только собственном.
    void reactionToggleRequested(qint64 id, const QString& emoji);

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    /// Перестраивает ряд чипов-реакций из reactions_ с нуля (issue
    /// #334) — общая часть конструктора и applyReactionChange(); проще
    /// пересобрать все чипы заново, чем инкрементально править один
    /// QPushButton, а реакций на одном сообщении всегда мало.
    void rebuildReactionChips();

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
    /// Текущее состояние реакций этой строки (issue #334) — источник
    /// истины для rebuildReactionChips(); обновляется на месте
    /// applyReactionChange(), а не пересоздаётся из внешнего списка
    /// каждый раз, поскольку ChatClient::reactionChanged() несёт только
    /// одну изменившуюся эмодзи за раз, не весь набор.
    QList<MessageReactionSummary> reactions_;
    QString currentUserLogin_;
    /// Контейнер под чипы-реакции — создаётся один раз в конструкторе и
    /// дальше только скрывается/показывается (setVisible()) и
    /// перестраивается изнутри (rebuildReactionChips()), а не
    /// создаётся/удаляется заново на каждое изменение.
    QWidget* reactionsRow_ = nullptr;
};

}  // namespace devicehub
