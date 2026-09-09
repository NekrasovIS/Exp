#pragma once

#include <QHash>
#include <QList>
#include <QPointer>
#include <QWidget>

#include "ui/ChatMessageRow.h"

class QImage;
class QLabel;
class QLineEdit;
class QPushButton;
class QScrollArea;
class QStackedWidget;
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

    /// Передаёт загруженное изображение вложения дальше в строку,
    /// которая его запросила (issue #188, см. previewAttachmentRequested())
    /// — ничего не делает, если та строка с тех пор исчезла (например,
    /// пользователь переключил канал раньше, чем пришёл ответ).
    void setAttachmentPreview(qint64 attachmentId, const QImage& image);

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

    /// Показывает "<login> is typing…" на несколько секунд, затем
    /// автоматически скрывает — MainWindow вызывает это из
    /// ChatClient::userTyping(). Issue #96: одновременно показывается
    /// только последний, о ком пришло сообщение о наборе текста
    /// (принятое упрощение первой версии — у chat-service нет сообщения
    /// "перестал печатать", поэтому без него нет чистого способа
    /// отслеживать набор одновременно печатающих).
    void showTypingUser(const QString& login);

    [[nodiscard]] QWidget* messagesContainer() const { return messagesContainer_; }
    [[nodiscard]] QLineEdit* messageEdit() const { return messageEdit_; }
    [[nodiscard]] QPushButton* sendButton() const { return sendButton_; }
    [[nodiscard]] QPushButton* attachButton() const { return attachButton_; }
    [[nodiscard]] QPushButton* callToggleButton() const { return callToggleButton_; }
    [[nodiscard]] QPushButton* searchButton() const { return searchButton_; }
    [[nodiscard]] QLabel* typingIndicatorLabel() const { return typingIndicatorLabel_; }
    [[nodiscard]] QPushButton* loadOlderButton() const { return loadOlderButton_; }
    /// Иконка "участники" в правом углу шапки (issue #184) — переключает
    /// видимость MemberListPanel, которой ChatView не владеет сама,
    /// поэтому только сигнализирует запрос, а не хранит состояние
    /// открыт/свёрнут самостоятельно.
    [[nodiscard]] QPushButton* memberListToggleButton() const { return memberListToggleButton_; }
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

    /// Испускается сразу при появлении строки с вложением-изображением
    /// (issue #188, см. isImageAttachment()) — MainWindow запускает
    /// фоновую загрузку через ChatRestClient::downloadAttachment() и
    /// передаёт результат обратно в setAttachmentPreview(), а не в
    /// обработчик "сохранить на диск", который тот же REST-вызов
    /// использует для настоящих кликов по "Download".
    void previewAttachmentRequested(qint64 attachmentId);

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

    /// Строит центрированную метку-разделитель дат (issue #188) —
    /// "Today"/"Yesterday"/полная дата в зависимости от того, на какой
    /// день приходится @p sentAt относительно текущей даты.
    [[nodiscard]] QLabel* buildDateSeparatorLabel(const QString& sentAt);

    /// Перерисовывает channelTitleLabel_ из currentChannelName_/encrypted_
    /// — используется совместно showChannel() и setEncrypted(), так что
    /// любой из них можно вызвать первым, не затерев эффект другого.
    void updateChannelTitleLabel();

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
};

}  // namespace devicehub
