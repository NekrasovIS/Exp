#pragma once

#include <QHash>
#include <QList>
#include <QStringList>
#include <QWidget>

#include "ui/ChatMessageRow.h"

class QHBoxLayout;
class QImage;
class QLabel;
class QLineEdit;
class QPushButton;
class QScrollArea;
class QSplitter;
class QStackedWidget;
class QTimer;
class QVBoxLayout;
class QVideoWidget;

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

    /// Обновляет кнопки Call/Leave и Mute/Unmute — MainWindow вызывает
    /// это после каждого изменения состояния CallManager (вход, выход,
    /// переключение mute), а не сам этот виджет отслеживает состояние
    /// звонка. Выход из звонка (inCall == false) также сбрасывает
    /// видео-полосу: скрывает её и убирает все удалённые плитки,
    /// поскольку видео не может пережить звонок, которому принадлежит.
    void setCallState(bool inCall, bool muted);

    /// Обновляет небольшую метку "кто в звонке".
    void setCallParticipants(const QStringList& participants);

    /// Обновляет кнопку переключения видео и видимость локального
    /// превью — MainWindow вызывает это после каждого
    /// CallManager::enableVideo()/disableVideo().
    void setVideoEnabled(bool enabled);

    /// То же самое, что setVideoEnabled(), для демонстрации экрана
    /// (issue #112) — MainWindow вызывает это после каждого
    /// CallManager::enableScreenShare()/disableScreenShare(). Включение
    /// одного всегда подразумевает, что другое теперь отключено (видео
    /// и демонстрация экрана в CallManager взаимоисключающие), поэтому
    /// вызывающий код должен вызывать оба сеттера вместе, а не полагаться
    /// здесь на то, что один подразумевает другой.
    void setScreenShareEnabled(bool enabled);

    /// Показывает (создавая плитку при первом вызове) последний
    /// декодированный кадр входящего видеотрека @p peerLogin.
    void showRemoteVideoFrame(const QString& peerLogin, const QImage& frame);

    /// Убирает видео-плитку @p peerLogin, если она есть.
    void removeRemoteVideo(const QString& peerLogin);

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
    [[nodiscard]] QPushButton* muteToggleButton() const { return muteToggleButton_; }
    [[nodiscard]] QPushButton* videoToggleButton() const { return videoToggleButton_; }
    [[nodiscard]] QPushButton* screenShareToggleButton() const { return screenShareToggleButton_; }
    [[nodiscard]] QPushButton* searchButton() const { return searchButton_; }
    [[nodiscard]] QLabel* callParticipantsLabel() const { return callParticipantsLabel_; }
    [[nodiscard]] QVideoWidget* localVideoWidget() const { return localVideoWidget_; }
    [[nodiscard]] QLabel* typingIndicatorLabel() const { return typingIndicatorLabel_; }
    [[nodiscard]] QPushButton* loadOlderButton() const { return loadOlderButton_; }
    /// Кнопка "Show/Hide Chat" (issue #153) — видна только во время
    /// звонка, когда область видео и панель чата делят QSplitter вместо
    /// того, чтобы чат всегда занимал одну и ту же фиксированную долю
    /// окна.
    [[nodiscard]] QPushButton* toggleChatVisibilityButton() const { return toggleChatVisibilityButton_; }

signals:
    /// Испускается при клике по кнопке "Create channel" на заглушке —
    /// MainWindow подключает это к той же обработке, что и собственная
    /// кнопка "+" у ChannelsPanel.
    void createChannelRequested();

    /// Клик по кнопке звонка — MainWindow решает, входить или выходить,
    /// на основе CallManager::inCall(), и вызывает обратно
    /// setCallState().
    void callToggleRequested();

    /// Клик по кнопке mute — тот же паттерн, что и callToggleRequested().
    void muteToggleRequested();

    /// Клик по кнопке переключения видео — тот же паттерн, что и
    /// callToggleRequested(): MainWindow решает включить или отключить
    /// на основе CallManager::videoEnabled() и вызывает обратно
    /// setVideoEnabled().
    void videoToggleRequested();

    /// Клик по кнопке переключения демонстрации экрана — тот же
    /// паттерн, что и videoToggleRequested().
    void screenShareToggleRequested();

    /// Клик по "Search" (issue #118) — MainWindow показывает/поднимает
    /// свой SearchDialog.
    void openSearchRequested();

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

    /// Клик по "Attach" (issue #116) — MainWindow открывает выбор
    /// файла, загружает выбранный файл через ChatRestClient, затем
    /// автоматически отправляет его как сообщение (см. doc-комментарий
    /// MainWindow::onAttachFileClicked()).
    void attachFileRequested();

    /// Клик по "Download" на сообщении с вложением — всплывает вверх
    /// из того ChatMessageRow, откуда пришёл.
    void downloadAttachmentRequested(qint64 attachmentId, const QString& filename);

private:
    /// Подключает editRequested()/deleteRequested() свежесозданной
    /// строки к собственному состоянию режима редактирования этого
    /// view / deleteMessageRequested() — используется совместно
    /// appendMessage() (пока единственное место, где создаются строки).
    void connectMessageRow(ChatMessageRow* row);

    /// Видимость локального превью/полосы — это "активно видео с
    /// камеры или демонстрация экрана" — пересчитывается из
    /// videoActive_/screenShareActive_, а не из собственного @p enabled
    /// каждого сеттера, так что вызов setVideoEnabled()/
    /// setScreenShareEnabled() в любом порядке после взаимоисключающего
    /// переключения всё равно оставляет видимым нужное.
    void updateLocalVideoVisibility();
    /// Перерисовывает channelTitleLabel_ из currentChannelName_/encrypted_
    /// — используется совместно showChannel() и setEncrypted(), так что
    /// любой из них можно вызвать первым, не затерев эффект другого.
    void updateChannelTitleLabel();
    /// Клик по "Hide Chat"/"Show Chat" (issue #153) — сворачивает/
    /// восстанавливает долю chatPanel_ в chatSplitter_, отдавая
    /// остальную часть окна области видео (или, если видео/демонстрация
    /// экрана уже не активны, просто пустому месту).
    void onToggleChatVisibilityClicked();


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
    QPushButton* muteToggleButton_ = nullptr;
    QPushButton* videoToggleButton_ = nullptr;
    QPushButton* screenShareToggleButton_ = nullptr;
    bool videoActive_ = false;
    bool screenShareActive_ = false;
    bool encrypted_ = false;
    QPushButton* searchButton_ = nullptr;
    QLabel* callParticipantsLabel_ = nullptr;
    /// Область видео (сверху) и chatPanel_ (снизу) — во время звонка
    /// область видео получает большую часть пространства, а chatPanel_
    /// можно свернуть через toggleChatVisibilityButton_ вместо
    /// фиксированной доли 50/50 (issue #153).
    QSplitter* chatSplitter_ = nullptr;
    /// История (loadOlderButton_/scrollArea_/typingIndicatorLabel_) плюс
    /// строка ввода сообщения — сгруппированы в один виджет, чтобы быть
    /// одним дочерним элементом QSplitter.
    QWidget* chatPanel_ = nullptr;
    QPushButton* toggleChatVisibilityButton_ = nullptr;
    bool chatPanelCollapsed_ = false;
    QWidget* videoStrip_ = nullptr;
    QHBoxLayout* videoStripLayout_ = nullptr;
    QVideoWidget* localVideoWidget_ = nullptr;
    QHash<QString, QLabel*> remoteVideoTiles_;
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
};

}  // namespace devicehub
