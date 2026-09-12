#pragma once

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>
#include <QWidget>

#include <optional>

class QAudioOutput;
class QBuffer;
class QImage;
class QLabel;
class QMediaPlayer;
class QPushButton;
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
/// когда у сообщения нет вложения (issue #116). isPinned (issue #338)
/// отражает состояние на момент загрузки/получения — ChatMessageRow
/// сама не резолвит его, только рисует то, что передано. reactions
/// пуст для сообщения, на которое пока никто не поставил реакцию
/// (issue #334).
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
    bool isPinned = false;
    QList<MessageReactionSummary> reactions;
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

/// True для голосовых сообщений (issue #359) — WAV-вложения, созданные
/// `VoiceMessageRecorder`. В отличие от произвольного вложения с
/// расширением `.wav`, отличить которое от голосового сообщения по
/// одному только имени файла в принципе нельзя, для этой версии
/// достаточно: рендерится Play/Pause вместо ссылки "Download" на любой
/// `.wav`, так же как рендерится превью на любое изображение.
///
/// issue #360 добавил сюда `.ogg`/`.m4a` (контейнеры, которые может
/// отдать `MediaRecorder` веб-клиента) тем же permissive-приёмом, что и
/// `.wav` — оба расширения больше никем в этом файле не заняты. `.webm`
/// — другое дело: этот же контейнер уже занят `isVideoAttachment()`
/// (issue #188), и большинство реальных `.webm`-вложений — видео, а не
/// голосовые сообщения, так что расширения одного самого по себе здесь
/// недостаточно — иначе обычное видео-вложение вида "clip.webm" начало
/// бы ошибочно показывать Play вместо предпросмотра видео. Поэтому для
/// `.webm` дополнительно требуется префикс имени файла
/// "voice-message-", которым и веб-, и десктопный рекордер всегда
/// называют результат записи (см. `MainWindow::onVoiceRecordToggled()`
/// и веб-клиента `useVoiceRecorder.ts`) — обычное видео с таким именем
/// пользователь вручную не назовёт.
[[nodiscard]] bool isAudioAttachment(const QString& filename);

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
    /// отдельный bool-флаг. @p canManageChannel (issue #338) — владелец
    /// канала/сообщества или модератор сообщества, владеющего каналом,
    /// куда идёт эта строка; управляет только видимостью Pin/Unpin в
    /// контекстном меню, не связано с @p isOwnMessage (закреплять/
    /// снимать может не автор, а тот, у кого есть эта роль — см.
    /// doc-комментарий класса ChatClient::sendPinMessage()).
    ChatMessageRow(const ChatMessage& message, bool showHeader, bool isOwnMessage,
                   const QString& currentUserLogin = QString(), bool canManageChannel = false,
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

    /// Передаёт скачанные байты голосового сообщения (issue #359) в
    /// проигрыватель и сразу начинает воспроизведение — вызывается
    /// ChatView в ответ на playbackRequested(). Ничего не делает, если
    /// у этой строки нет вложения-голосового сообщения (см.
    /// isAudioAttachment()) — тот же принцип защитной проверки, что и у
    /// setAttachmentPreview() выше.
    void setAudioData(const QByteArray& data);

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

    /// Выбор эмодзи в подменю "React" контекстного меню, либо клик по
    /// уже существующему чипу-реакции под баблом (issue #334) — оба
    /// пути ведут к одному и тому же переключению (toggle) на стороне
    /// сервера, поэтому оба эмитят один и тот же сигнал. Доступно на
    /// любом сообщении, не только собственном.
    void reactionToggleRequested(qint64 id, const QString& emoji);

    /// Выбор "Reply" в контекстном меню по правому клику (issue #306) —
    /// доступно на любом сообщении, не только собственном, в отличие от
    /// editRequested/deleteRequested. Слушатель (ChatView) сам решает,
    /// что показать как "цитата" в поле ввода — эта строка передаёт
    /// только id.
    void replyRequested(qint64 id);

    /// Клик по "▶ Play" на голосовом сообщении (issue #359), когда
    /// аудио ещё не загружено этой строкой — вызывающий код запускает
    /// скачивание и позже вызывает setAudioData(). Повторный клик по
    /// Play/Pause после того, как аудио уже загружено, ничего не
    /// эмиттит — переключает уже созданный проигрыватель напрямую, без
    /// повторного похода за данными.
    void playbackRequested(qint64 attachmentId);

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
    /// Кнопка "▶ Play"/"⏸ Pause" вложения-голосового сообщения (issue
    /// #359, см. isAudioAttachment()) — null для строк без такого
    /// вложения. audioPlayer_/audioOutput_/audioBuffer_ создаются лениво
    /// в setAudioData(), а не в конструкторе — до первого клика по Play
    /// байтов ещё нет, а до setAudioData() создавать проигрыватель не
    /// для чего.
    QPushButton* playButton_ = nullptr;
    QMediaPlayer* audioPlayer_ = nullptr;
    QAudioOutput* audioOutput_ = nullptr;
    /// QBuffer::setData() копирует байты во внутреннее хранилище самого
    /// буфера (не хранит указатель на внешний QByteArray) — умышленно,
    /// чтобы не зависеть от порядка разрушения QObject-детей этой
    /// строки и отдельного QByteArray-члена.
    QBuffer* audioBuffer_ = nullptr;
    qint64 attachmentId_ = -1;
    /// Issue #338 — управляет видимостью Pin/Unpin в контекстном меню,
    /// построенном лениво при каждом правом клике (см. конструктор), а
    /// не пересобираемом при setPinned().
    bool canManageChannel_ = false;
    bool isPinned_ = false;
    /// Виден только пока isPinned_ true — null до первого раза, когда
    /// это стало нужным, создаётся лениво в setPinned() ИЛИ в
    /// конструкторе, если сообщение изначально уже закреплено.
    QLabel* pinnedIndicatorLabel_ = nullptr;
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
