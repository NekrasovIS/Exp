#pragma once

#include <QHash>
#include <QList>
#include <QPointer>
#include <QWidget>

#include "ui/ChatMessageRow.h"

class QCompleter;
class QImage;
class QLabel;
class QLineEdit;
class QPushButton;
class QScrollArea;
class QStackedWidget;
class QStringListModel;
class QTimer;
class QVBoxLayout;

namespace devicehub {

/**
 * @brief Основная область содержимого: показывает заглушку, пока в
 *        ChannelsPanel не выбран канал, а затем сгруппированный список
 *        сообщений с аватарами и поле отправки для этого канала.
 *
 * Чистое представление — MainWindow владеет ChatClient и передаёт
 * сообщения через appendMessage()/appendSystemLine()/clearLog(); этот
 * класс отвечает только за layout, переключение заглушка/канал и
 * решение (через chat_message_grouping), нужен ли сообщению собственный
 * заголовок.
 */
class ChatView : public QWidget {
    Q_OBJECT

public:
    explicit ChatView(QWidget* parent = nullptr);

    /// Возвращает обратно к заглушке "канал не выбран".
    void showPlaceholder();

    /// Переключает на страницу чата и выставляет заголовок @p channelName.
    void showChannel(const QString& channelName);

    /// Отмечает, зашифрован ли текущий открытый канал (issue #138) —
    /// добавляет к заголовку значок замка и отключает Attach/Search (не
    /// поддерживаются для зашифрованных каналов на этом этапе, поэтому
    /// отключены, а не оставлены падать на стороне сервера). Вызывать
    /// после showChannel() для того канала, к которому это относится.
    void setEncrypted(bool encrypted);

    /// Переключает вид кнопки записи между "🎤" (не идёт запись) и "⏹
    /// Stop" (идёт) — MainWindow вызывает это в ответ на собственное
    /// решение start()/stop() у VoiceMessageRecorder, а не наоборот:
    /// ChatView не владеет записывающим устройством, только отражает
    /// его состояние (issue #359).
    void setRecordingVoice(bool recording);

    /// Нужно, чтобы решить, является ли добавляемое сообщение "своим"
    /// (пузырь выровнен вправо, акцентный цвет, без аватара) или чужим.
    void setCurrentUserLogin(const QString& login);

    /// Владеет ли вошедший пользователь текущим открытым каналом (или
    /// его сообществом), либо является модератором сообщества (issue
    /// #338) — управляет видимостью Pin/Unpin у каждой новой строки.
    /// MainWindow вызывает это при открытии канала, как только известны
    /// и владелец сообщества, и (асинхронно) список модераторов; строки,
    /// уже построенные до того, как это значение стало известно, не
    /// получают Pin/Unpin ретроактивно — принятое упрощение первой
    /// версии, см. doc-комментарий MainWindow.
    void setCanManageChannel(bool canManage);

    /// Добавляет настоящее сообщение чата — группируется с предыдущим
    /// (без повтора аватара/имени/времени), если они от одного автора
    /// и в пределах нескольких минут друг от друга.
    void appendMessage(const ChatMessage& message);

    /// Вставляет @p messages (в хронологическом порядке, от старых к
    /// новым) в начало списка, над уже показанным — "Load older
    /// messages" из issue #100. Группировка между ними считается только
    /// внутри этой пачки (сравнение с тем, что было ранее самым старым
    /// показанным сообщением, намеренно пропускается — см.
    /// doc-комментарий класса). В отличие от appendMessage(), сохраняет
    /// текущую позицию прокрутки пользователя, а не прыгает вниз.
    void prependMessages(const QList<ChatMessage>& messages);

    /// Показывает/скрывает кнопку "Load older messages" над списком
    /// сообщений — MainWindow вызывает это с признаком того, выглядела
    /// ли последняя загруженная страница истории так, будто есть ещё
    /// (пришла полной).
    void setLoadOlderVisible(bool visible);

    /// Обновляет отображаемый текст сообщения @p id на месте (issue
    /// #107) — ничего не делает, если это сообщение сейчас не
    /// показано (например, прокручено за пределы страницы истории,
    /// которая с тех пор была заменена).
    void updateMessageBody(qint64 id, const QString& newBody);

    /// Полностью удаляет строку @p id, если она сейчас показана.
    void removeMessage(qint64 id);

    /// Обновляет закреплённость строки @p id на месте (issue #338) —
    /// ничего не делает, если сообщение сейчас не показано, тот же
    /// принцип, что и у updateMessageBody().
    void updatePinned(qint64 id, bool isPinned);

    /// Заменяет счётчик на кнопке закреплённых сообщений (issue #338) —
    /// кнопка скрыта при 0. MainWindow вызывает это при открытии канала
    /// (после listPinnedMessages()) и на каждое messagePinned()/
    /// messageUnpinned().
    void setPinnedMessagesCount(int count);

    /// Применяет одно изменение реакции к строке @p id (issue #334) —
    /// ничего не делает, если это сообщение сейчас не показано (тот же
    /// принцип, что и у updateMessageBody()). @p logins — полный список
    /// для @p emoji после переключения, как приходит из
    /// ChatClient::reactionChanged(), не дельта.
    void updateReactions(qint64 id, const QString& emoji, const QStringList& logins);

    /// Передаёт загруженное изображение вложения дальше в строку,
    /// которая его запросила (issue #188, см. previewAttachmentRequested())
    /// — ничего не делает, если та строка с тех пор исчезла (например,
    /// пользователь переключил канал раньше, чем пришёл ответ).
    void setAttachmentPreview(qint64 attachmentId, const QImage& image);

    /// Передаёт скачанные байты голосового сообщения дальше в строку,
    /// которая их запросила (issue #359, см. voicePlaybackRequested())
    /// — ничего не делает, если та строка с тех пор исчезла, тот же
    /// принцип, что и у setAttachmentPreview() выше.
    void setVoiceMessageData(qint64 attachmentId, const QByteArray& data);

    /// Прокручивает к строке @p id, если она сейчас показана (issue
    /// #118, переход к результату поиска) — @return false, если это
    /// сообщение сейчас не загружено (например, дальше в истории, чем
    /// успел загрузить "Load older").
    bool scrollToMessage(qint64 id);

    /// True, пока поле отправки редактирует существующее сообщение, а
    /// не составляет новое — устанавливается кликом по собственной
    /// кнопке "Edit" сообщения, сбрасывается cancelEditingMessage() или
    /// (со стороны MainWindow) после того, как правка фактически
    /// отправлена. -1, если редактирование не идёт.
    [[nodiscard]] qint64 editingMessageId() const { return editingMessageId_; }

    /// Выходит из режима редактирования: сбрасывает editingMessageId_,
    /// восстанавливает обычный текст кнопки отправки и очищает поле
    /// сообщения.
    void cancelEditingMessage();

    /// Добавляет приглушённую, центрированную системную/статусную строку
    /// (подписка, ошибки, ...) — всегда прерывает текущую группировку
    /// сообщений, так что следующее настоящее сообщение получает
    /// собственный заголовок независимо от автора.
    void appendSystemLine(const QString& text);

    /// Очищает список сообщений и сбрасывает состояние группировки.
    void clearLog();

    /// Обновляет текст кнопки Call/Leave — MainWindow вызывает это
    /// после каждого входа/выхода из звонка. Элементы управления самим
    /// звонком (mute, видео, демонстрация экрана, видео-плитки) больше
    /// не в ChatView — см. CallWindow (issue #185), которую MainWindow
    /// показывает/скрывает вместе с этим состоянием.
    void setCallState(bool inCall);

    /// Помечает сообщение @p id как цель ответа (issue #306) — показывает
    /// строку "Replying to ..." над композером с автором/фрагментом
    /// текста оригинала из messagesById_. Ничего не делает, если @p id
    /// сейчас не в этом кэше — в норме не должно происходить, поскольку
    /// ChatMessageRow эмитит replyRequested только для строк, которые
    /// сама ChatView только что построила и закэшировала. Отменяет
    /// текущее редактирование, если оно шло — это два взаимоисключающих
    /// режима композера.
    void setReplyTarget(qint64 id);

    /// Снимает текущую цель ответа и скрывает строку "Replying to ...",
    /// не отправляя сообщение — клик по кнопке отмены рядом с ней.
    void clearReplyTarget();

    /// Возвращает id текущей цели ответа (-1, если её нет) и сразу же
    /// снимает её. MainWindow вызывает это ровно один раз при
    /// фактической отправке сообщения, чтобы прикрепить
    /// reply_to_message_id к исходящему кадру и не оставить старую цель
    /// висящей на следующее сообщение.
    [[nodiscard]] qint64 consumeReplyTarget();

    /// Показывает "<login> is typing…" на несколько секунд, затем
    /// автоматически скрывает — MainWindow вызывает это из
    /// ChatClient::userTyping(). Issue #96: одновременно показывается
    /// только последний, о ком пришло сообщение о наборе текста
    /// (принятое упрощение первой версии — у chat-service нет сообщения
    /// "перестал печатать", поэтому без него нет чистого способа
    /// отслеживать набор одновременно печатающих).
    void showTypingUser(const QString& login);

    /// Список логинов участников текущего сообщества (issue #326) —
    /// MainWindow передаёт то же самое, что уже приходит из
    /// ChatRestClient::listMembers() для MemberListPanel, без
    /// отдельного REST-запроса специально под автокомплит. Используется
    /// только для фильтрации подсказок @упоминания в поле сообщения;
    /// подсветка самих упоминаний (issue #307) в этот список не смотрит
    /// вообще.
    void setChannelMemberLogins(const QStringList& logins);

    [[nodiscard]] QWidget* messagesContainer() const { return messagesContainer_; }
    [[nodiscard]] QLineEdit* messageEdit() const { return messageEdit_; }
    [[nodiscard]] QPushButton* sendButton() const { return sendButton_; }
    [[nodiscard]] QPushButton* attachButton() const { return attachButton_; }
    /// Кнопка записи голосового сообщения (issue #359) — рядом с
    /// attachButton() в композере.
    [[nodiscard]] QPushButton* recordVoiceButton() const { return recordVoiceButton_; }
    [[nodiscard]] QPushButton* callToggleButton() const { return callToggleButton_; }
    [[nodiscard]] QPushButton* searchButton() const { return searchButton_; }
    [[nodiscard]] QLabel* typingIndicatorLabel() const { return typingIndicatorLabel_; }
    [[nodiscard]] QPushButton* loadOlderButton() const { return loadOlderButton_; }
    /// Иконка "участники" в правом углу шапки (issue #184) — переключает
    /// видимость MemberListPanel, которой ChatView не владеет сама,
    /// поэтому только сигнализирует запрос, а не хранит состояние
    /// открыт/свёрнут самостоятельно.
    [[nodiscard]] QPushButton* memberListToggleButton() const { return memberListToggleButton_; }
    /// Автокомплит @упоминаний (issue #326) — тесты проверяют через это
    /// содержимое всплывающего списка/текущий префикс фильтрации, а не
    /// через реальное открытие всплывающего окна (как и везде в этом
    /// проекте, взаимодействие с настоящим модальным/всплывающим окном
    /// не эмулируется в юнит-тестах).
    [[nodiscard]] QCompleter* mentionCompleter() const { return mentionCompleter_; }
    /// Кнопка "📌 N" в шапке (issue #338) — скрыта, пока в канале нет
    /// закреплённых сообщений (см. setPinnedMessagesCount()).
    [[nodiscard]] QPushButton* pinnedMessagesButton() const { return pinnedMessagesButton_; }

signals:
    /// Испускается при клике по кнопке "Create channel" на заглушке —
    /// MainWindow подключает это к той же обработке, что и собственная
    /// кнопка "+" у ChannelsPanel.
    void createChannelRequested();

    /// Клик по кнопке звонка — MainWindow решает, входить или выходить,
    /// на основе CallManager::inCall(), и вызывает обратно
    /// setCallState(). Дальнейшее управление уже идущим звонком (mute,
    /// видео, демонстрация экрана, выход) — через CallWindow, не отсюда.
    void callToggleRequested();

    /// Клик по "Search" (issue #118) — MainWindow показывает/поднимает
    /// свой SearchDialog.
    void openSearchRequested();

    /// Клик по иконке "участники" (issue #184) — MainWindow переключает
    /// видимость своей MemberListPanel; сама ChatView этой панелью не
    /// владеет и не знает, открыта она сейчас или нет.
    void memberListToggleRequested();

    /// Пользователь печатает в поле сообщения — с ограничением частоты
    /// (не чаще одного раза за окно охлаждения), а не при каждом
    /// нажатии клавиши, чтобы ChatClient::sendTyping() из MainWindow не
    /// заваливал сеть.
    void typingRequested();

    /// Клик по "Load older messages" — MainWindow запрашивает
    /// следующую страницу перед самым старым сообщением, которое
    /// сейчас есть у ChatView.
    void loadOlderMessagesRequested();

    /// Клик по "Delete" на одном из собственных сообщений пользователя.
    void deleteMessageRequested(qint64 id);

    /// Выбор "Pin"/"Unpin" в контекстном меню сообщения (issue #338) —
    /// доступно только когда setCanManageChannel(true).
    void pinMessageRequested(qint64 id);
    void unpinMessageRequested(qint64 id);

    /// Клик по кнопке "📌 N" в шапке (issue #338) — MainWindow
    /// показывает/поднимает свой PinnedMessagesDialog.
    void pinnedMessagesToggleRequested();

    /// Клик по "Attach" (issue #116) — MainWindow открывает выбор
    /// файла, загружает выбранный файл через ChatRestClient, затем
    /// автоматически отправляет его как сообщение (см. doc-комментарий
    /// MainWindow::onAttachFileClicked()).
    void attachFileRequested();

    /// Клик по "Download" на сообщении с вложением — всплывает вверх
    /// из того ChatMessageRow, откуда пришёл.
    void downloadAttachmentRequested(qint64 attachmentId, const QString& filename);

    /// Выбор эмодзи в подменю "React" либо клик по уже существующему
    /// чипу-реакции — всплывает вверх из того ChatMessageRow, откуда
    /// пришёл (issue #334). MainWindow вызывает
    /// ChatClient::sendToggleReaction().
    void reactionToggleRequested(qint64 id, const QString& emoji);

    /// Испускается сразу при появлении строки с вложением-изображением
    /// (issue #188, см. isImageAttachment()) — MainWindow запускает
    /// фоновую загрузку через ChatRestClient::downloadAttachment() и
    /// передаёт результат обратно в setAttachmentPreview(), а не в
    /// обработчик "сохранить на диск", который тот же REST-вызов
    /// использует для настоящих кликов по "Download".
    void previewAttachmentRequested(qint64 attachmentId);

    /// Клик по кнопке записи голосового сообщения (issue #359) —
    /// переключает запись старт/стоп; MainWindow владеет самим
    /// VoiceMessageRecorder и решает, что делать с готовым WAV на
    /// стороне stop() (см. doc-комментарий MainWindow::
    /// onRecordVoiceToggleClicked()).
    void recordVoiceToggleRequested();

    /// Клик по "▶ Play" на голосовом сообщении, когда аудио ещё не
    /// загружено этой строкой (issue #359) — тот же принцип, что и у
    /// previewAttachmentRequested() выше, MainWindow скачивает через
    /// ChatRestClient::downloadAttachment() и передаёт результат
    /// обратно в setVoiceMessageData().
    void voicePlaybackRequested(qint64 attachmentId);

protected:
    /// Перехватывает Enter/Tab/Escape у messageEdit_, пока всплывающий
    /// список автокомплита (issue #326) открыт, чтобы они выбирали
    /// подсказку вместо того, чтобы одновременно ещё и отправлять
    /// сообщение (returnPressed уже подключён к sendButton_->click()) —
    /// тот же приём, что в официальном примере Qt Custom Completer.
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    /// Подключает editRequested()/deleteRequested() свежесозданной
    /// строки к собственному состоянию режима редактирования этого
    /// view / deleteMessageRequested() — используется совместно
    /// appendMessage() (пока единственное место, где создаются строки).
    void connectMessageRow(ChatMessageRow* row);

    /// Если у @p message есть вложение-изображение (issue #188),
    /// запоминает @p row в pendingImagePreviewRows_ и испускает
    /// previewAttachmentRequested() — общая часть appendMessage()/
    /// prependMessages(), оба создают строки одинаково.
    void requestPreviewIfImageAttachment(const ChatMessage& message, ChatMessageRow* row);

    /// Возвращает копию @p message с заполненными replyToAuthor/
    /// replyToBodySnippet (issue #306), если message.replyToMessageId
    /// найден в messagesById_ — общая часть appendMessage()/
    /// prependMessages(). Оставляет оба поля пустыми (ChatMessageRow
    /// покажет заглушку "unavailable"), если не найден, и возвращает
    /// @p message без изменений, если replyToMessageId < 0.
    [[nodiscard]] ChatMessage resolveReplyPreview(const ChatMessage& message) const;

    /// Строит центрированную метку-разделитель дат (issue #188) —
    /// "Today"/"Yesterday"/полная дата в зависимости от того, на какой
    /// день приходится @p sentAt относительно текущей даты.
    [[nodiscard]] QLabel* buildDateSeparatorLabel(const QString& sentAt);

    /// Перерисовывает channelTitleLabel_ из currentChannelName_/encrypted_
    /// — используется совместно showChannel() и setEncrypted(), так что
    /// любой из них можно вызвать первым, не затерев эффект другого.
    void updateChannelTitleLabel();

    /// Пересчитывает подсказки автокомплита (issue #326) по текущему
    /// тексту/позиции курсора messageEdit_ — вызывается на каждое
    /// textEdited(). Находит "@", перед которым начинается слово
    /// (начало строки либо пробел), фильтрует channelMemberLogins_ по
    /// набранному после "@" префиксу и, если что-то нашлось,
    /// запоминает позицию "@" в mentionTriggerPos_ и открывает
    /// mentionCompleter_ рядом с курсором; иначе скрывает его попап.
    void updateMentionAutocomplete();

    /// Подставляет выбранную подсказку @p login на место "@<префикс>"
    /// в messageEdit_ (используя mentionTriggerPos_) — общий обработчик
    /// и для QCompleter::activated(), и (в тестах) для прямого вызова.
    void insertMentionCompletion(const QString& login);

    QStackedWidget* stack_ = nullptr;
    QLabel* channelTitleLabel_ = nullptr;
    /// Обычное имя канала, без префикса-замка от setEncrypted() —
    /// showChannel() устанавливает это; setEncrypted() заново выводит
    /// текст метки из этого значения, так что оба метода можно вызывать
    /// в любом порядке.
    QString currentChannelName_;
    QScrollArea* scrollArea_ = nullptr;
    QWidget* messagesContainer_ = nullptr;
    QVBoxLayout* messagesLayout_ = nullptr;
    QPushButton* loadOlderButton_ = nullptr;
    QLineEdit* messageEdit_ = nullptr;
    QPushButton* sendButton_ = nullptr;
    QPushButton* attachButton_ = nullptr;
    /// Кнопка записи голосового сообщения (issue #359), рядом с
    /// attachButton_ в композере.
    QPushButton* recordVoiceButton_ = nullptr;
    QPushButton* callToggleButton_ = nullptr;
    bool encrypted_ = false;
    QPushButton* searchButton_ = nullptr;
    QPushButton* memberListToggleButton_ = nullptr;
    QPushButton* pinnedMessagesButton_ = nullptr;
    /// Issue #338 — см. doc-комментарий setCanManageChannel().
    bool canManageChannel_ = false;
    QLabel* typingIndicatorLabel_ = nullptr;
    /// Виден только пока editingMessageId_ >= 0 — единственный оставшийся
    /// индикатор режима редактирования с тех пор, как sendButton_ стал
    /// иконкой без текста (issue #182).
    QLabel* editingIndicatorLabel_ = nullptr;
    QTimer* typingIndicatorHideTimer_ = nullptr;
    QTimer* typingThrottleTimer_ = nullptr;
    /// Полоса "Replying to ..." над композером (issue #306) — видна
    /// только пока replyTargetId_ >= 0.
    QWidget* replyBar_ = nullptr;
    QLabel* replyBarLabel_ = nullptr;
    qint64 replyTargetId_ = -1;
    bool hasLastMessage_ = false;
    ChatMessage lastMessage_;
    QString currentUserLogin_;
    /// True, пока полоса прокрутки находится (или почти находится)
    /// внизу — новые сообщения удерживают её там, как ведёт себя
    /// обычный чат, но это прекращается, как только пользователь
    /// прокручивает вверх для чтения истории, и обновляется по мере
    /// дальнейшей прокрутки. См. подключение rangeChanged/valueChanged
    /// в конструкторе.
    bool stickToBottom_ = true;
    qint64 editingMessageId_ = -1;
    /// Строки, ожидающие ответа на previewAttachmentRequested() (issue
    /// #188) — QPointer, а не голый указатель, поскольку строка вполне
    /// может исчезнуть (переключение канала -> clearLog(), удаление
    /// сообщения) раньше, чем придёт ответ.
    QHash<qint64, QPointer<ChatMessageRow>> pendingImagePreviewRows_;
    /// Строки, ожидающие ответа на voicePlaybackRequested() (issue
    /// #359) — тот же принцип QPointer, что и у
    /// pendingImagePreviewRows_ выше, но заполняется лениво по клику на
    /// "Play", а не сразу для каждой строки с голосовым сообщением.
    QHash<qint64, QPointer<ChatMessageRow>> pendingVoicePlaybackRows_;
    /// Логины участников текущего сообщества (issue #326) — только для
    /// фильтрации автокомплита, см. setChannelMemberLogins().
    QStringList channelMemberLogins_;
    QCompleter* mentionCompleter_ = nullptr;
    /// Живёт внутри mentionCompleter_ (тот же родитель) — обновляется
    /// на месте через setStringList() в setChannelMemberLogins(),
    /// вместо пересоздания модели/completer'а при каждом обновлении
    /// списка участников.
    QStringListModel* mentionModel_ = nullptr;
    /// Позиция символа "@", с которого начинается сейчас набираемое
    /// упоминание, в тексте messageEdit_ — -1, когда автокомплит не
    /// активен. Используется insertMentionCompletion() при подстановке.
    int mentionTriggerPos_ = -1;
    /// Кэш уже показанных сообщений по id (issue #306) — используется,
    /// чтобы резолвить автора/фрагмент текста для цитаты-ответа (как у
    /// новых сообщений, так и для строки "Replying to ..." при выборе
    /// цели ответа). Не растёт неограниченно: очищается в clearLog() при
    /// каждом переключении канала, как и сам список показанных строк.
    QHash<qint64, ChatMessage> messagesById_;
};

}  // namespace devicehub
