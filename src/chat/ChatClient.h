#pragma once

#include <QJsonObject>
#include <QObject>
#include <QStringList>
#include <QUrl>
#include <QWebSocket>

namespace devicehub {

/**
 * @brief Соединение с каналом в реальном времени к chat-service через
 *        WebSocket.
 *
 * Протокол: после connectToChannel() первым отправляется кадр
 * `{"token", "channel_id"}`; как только chat-service подтверждает
 * `{"subscribed": true}`, sendMessage() отправляет кадры `{"body"}`, а
 * входящие рассылки (от любого подписчика, включая этот клиент)
 * вызывают messageReceived(). Асинхронно через сигналы, как AuthClient
 * и классы в devices/ — никогда не блокирует GUI-поток.
 *
 * Также отражает релей сигналинга группового голосового звонка на
 * chat-service (issue #46): joinCall()/leaveCall() отправляют
 * `{"call_join"}`/`{"call_leave"}`, sendCallSignal() отправляет
 * `{"call_signal": {"to", "payload"}}` (непрозрачный payload SDP/ICE,
 * который этот класс никогда не инспектирует), а соответствующие
 * сигналы callRosterReceived/callPeerJoined/callPeerLeft/
 * callSignalReceived срабатывают для соответствующих серверных кадров.
 * Валидно только после того, как сработал subscribed().
 *
 * sendCallReaction() (issue #312) отправляет `{"call_reaction":
 * "<emoji>"}`; callReactionReceived() срабатывает на рассылку
 * `{"call_reaction": {"login", "emoji"}}` от другого участника звонка
 * (chat-service никогда не отправляет это эхом обратно отправителю, тот
 * же принцип, что и у typing). Оба валидны только пока этот клиент сам
 * сейчас в звонке (joinCall() уже вызван, leaveCall() ещё нет).
 *
 * Прокси-сигналинг SFU (issue #232) — параллельный, более новый путь
 * поверх того же WebSocket: sendJanusAttach()/sendJanusMessage()
 * отправляют `{"janus_attach"}`/`{"janus_message": {"handle", "body",
 * "jsep"?}}`, chat-service пересылает их напрямую своей Janus-сессии
 * для этого подключения (см. WebSocketServer, там же и объяснение,
 * почему Janus никогда не виден клиенту напрямую). janusAttached()/
 * janusMessageAck()/janusEventReceived() — соответствующие ответы;
 * sfuRoomAssigned() — id комнаты Janus для звонка, отдельным сигналом
 * рядом с callRosterReceived() на тот же ответ joinCall().
 *
 * sendTyping() (issue #96) отправляет `{"typing": true}`; userTyping()
 * срабатывает на соответствующую рассылку `{"user_typing": "<login>"}`
 * от другого подписчика (chat-service никогда не отправляет это эхом
 * обратно отправителю). Эфемерно, как и присутствие в звонке — сообщения
 * «перестал печатать» не существует, ChatView вместо этого сам гасит
 * свой индикатор по таймауту.
 *
 * sendEditMessage()/sendDeleteMessage() (issue #107) отправляют
 * `{"edit_message": {"id", "body"}}`/`{"delete_message": {"id"}}` —
 * chat-service разрешает редактировать/удалять сообщение только его
 * собственному автору (никогда владельцу канала/сообщества, действующему
 * от имени чужого сообщения), при отказе об этом сообщается через
 * errorOccurred(). При успехе каждый подписчик (включая
 * редактирующего/удаляющего) получает messageEdited()/messageDeleted() —
 * тот же паттерн «рассылка всем, без локального оптимистичного
 * обновления», что уже использует messageReceived() для новых
 * сообщений.
 *
 * Опциональный параметр @p attachmentId у sendMessage() (issue #116)
 * ссылается на файл, уже загруженный через
 * ChatRestClient::uploadAttachment() — этот класс никогда сам не
 * трогает байты вложения, только id/имя файла, которые едут вместе с
 * messageReceived().
 *
 * connectToDirectMessageThread() (issue #187, Фаза 2b) — альтернатива
 * connectToChannel() для живой доставки личных сообщений: тот же кадр
 * Hello, но с `dm_thread_id` вместо `channel_id`, и тот же
 * subscribed()/messageReceived() на ответ (DirectMessage не несёт
 * attachment_id/attachment_filename — messageReceived() получит для
 * них -1/пустую строку, как и для обычного сообщения без вложения).
 * Никаких кадров звонка/typing/edit_message/delete_message для диалога
 * не отправлять — chat-service не обрабатывает их для подписки на
 * личный диалог. Использовать отдельный экземпляр ChatClient для
 * диалогов, не тот же самый, что подписан на канал (нужен независимый
 * WebSocket, чтобы диалог не занимал место активной подписки на канал,
 * от которой также зависит групповой звонок в CallManager).
 */
class ChatClient : public QObject {
    Q_OBJECT

public:
    explicit ChatClient(QUrl webSocketUrl, QObject* parent = nullptr);

    /// Открывает соединение и подписывается на @p channelId, используя @p token.
    void connectToChannel(const QString& token, qint64 channelId);

    /// Открывает соединение и подписывается на личный диалог @p threadId
    /// (issue #187, Фаза 2b) — см. doc-комментарий класса.
    void connectToDirectMessageThread(const QString& token, qint64 threadId);

    /// Отправляет @p body в канал, на который подписан этот клиент,
    /// опционально ссылаясь на уже загруженный @p attachmentId
    /// (issue #116) — -1 (значение по умолчанию) означает отсутствие
    /// вложения.
    void sendMessage(const QString& body, qint64 attachmentId = -1);

    /// Закрывает соединение — независимо от того, был ли этот клиент
    /// подписан на канал или на личный диалог.
    void disconnectFromChannel();

    /// Присоединяется к голосовому звонку в подписанном канале;
    /// вызывает ответ callRosterReceived().
    void joinCall();

    /// Покидает голосовой звонок в подписанном канале.
    void leaveCall();

    /// Ретранслирует непрозрачный сигналинговый @p payload (SDP
    /// offer/answer или ICE-кандидат) участнику звонка @p to.
    void sendCallSignal(const QString& to, const QJsonObject& payload);

    /// Лёгкая эмодзи-реакция во время звонка (issue #312) — валидно
    /// только пока joinCall() уже вызван и leaveCall() ещё нет; вызывает
    /// errorOccurred(), если этот клиент сейчас не в звонке. Рассылает
    /// callReactionReceived() остальным участникам, никогда себе.
    void sendCallReaction(const QString& emoji);

    /// Прокси-сигналинг SFU (issue #232): attach'ит новый handle плагина
    /// videoroom на Janus-сессии этого WS-подключения (chat-service
    /// создаёт саму сессию при самом первом вызове за время жизни
    /// соединения — см. doc-комментарий класса). Вызывает ответ
    /// janusAttached().
    void sendJanusAttach();

    /// Пересылает @p body (+опционально @p jsep, если не пустой) как
    /// есть указанному Janus-@p handle через chat-service — этот класс
    /// никогда не разбирает их содержимое ("join"/"configure"/
    /// "subscribe"/"start" и т.п., см. протокол Janus videoroom).
    /// Прямой ответ (может быть просто подтверждением, реальный результат
    /// приходит асинхронно) — через janusMessageAck(); асинхронные
    /// события той же сессии (в т.ч. jsep-answer от Janus) — через
    /// janusEventReceived().
    void sendJanusMessage(qint64 handle, const QJsonObject& body, const QJsonObject& jsep = QJsonObject());

    /// Сообщает chat-service, что локальный пользователь печатает в
    /// подписанном канале — вызывает userTyping() у всех остальных
    /// подписчиков.
    void sendTyping();

    /// Запрашивает редактирование сообщения @p id (должно принадлежать
    /// этому пользователю) на @p newBody.
    void sendEditMessage(qint64 id, const QString& newBody);

    /// Запрашивает удаление сообщения @p id (должно принадлежать этому
    /// пользователю).
    void sendDeleteMessage(qint64 id);

signals:
    /// Испускается, как только chat-service подтверждает подписку —
    /// @p id это channelId или dmThreadId, в зависимости от того, какой
    /// из connectToChannel()/connectToDirectMessageThread() был вызван.
    void subscribed(qint64 id);

    /// Испускается для каждого сообщения, разосланного в подписанном
    /// канале. @p attachmentId равен -1, а @p attachmentFilename пуст,
    /// когда у сообщения нет вложения (issue #116).
    void messageReceived(qint64 id, const QString& author, const QString& body, const QString& sentAt,
                          qint64 attachmentId, const QString& attachmentFilename);

    /// Сообщение было отредактировано — @p editedAt — новая метка
    /// времени редактирования (сериализована Postgres, в том же
    /// формате, что и sentAt).
    void messageEdited(qint64 id, const QString& newBody, const QString& editedAt);

    /// Сообщение было удалено.
    void messageDeleted(qint64 id);

    void errorOccurred(const QString& message);

    /// Ответ на joinCall(): уже существующие участники звонка, не
    /// включая себя.
    void callRosterReceived(const QStringList& participants);

    /// SFU-комната для этого звонка (issue #123/#230/#231/#232) — id
    /// videoroom-комнаты Janus для подписанного канала, поле "sfu_room"
    /// в том же ответе на joinCall(), что и callRosterReceived(), но
    /// отдельным сигналом, чтобы не трогать её уже существующую
    /// сигнатуру. Не испускается, если chat-service не смог обеспечить
    /// комнату (Janus временно недоступен) — CallManager в этом случае
    /// просто не получает SFU-путь для этого звонка.
    void sfuRoomAssigned(const QString& room);

    /// Ещё один участник присоединился к звонку.
    void callPeerJoined(const QString& login);

    /// Участник покинул звонок (явный leaveCall() или отключение).
    void callPeerLeft(const QString& login);

    /// Сигналинговый payload, ретранслированный от другого участника
    /// звонка.
    void callSignalReceived(const QString& from, const QJsonObject& payload);

    /// Другой участник звонка отправил эмодзи-реакцию (issue #312).
    void callReactionReceived(const QString& login, const QString& emoji);

    /// Ответ на sendJanusAttach() — @p handle нового handle'а плагина
    /// videoroom.
    void janusAttached(qint64 handle);

    /// Прямой ответ chat-service на sendJanusMessage() — может быть
    /// просто подтверждением приёма (реальный результат см.
    /// janusEventReceived()).
    void janusMessageAck(const QJsonObject& response);

    /// Асинхронное событие Janus-сессии этого подключения (issue #232) —
    /// jsep-answer на configure(), приглашение подписаться на нового
    /// publisher-а и т.п. Пересылается дословно, как получено от Janus
    /// через chat-service.
    void janusEventReceived(const QJsonObject& event);

    /// Другой подписчик печатает в подписанном канале.
    void userTyping(const QString& login);

private:
    void onConnected();
    void onTextMessageReceived(const QString& message);

    QUrl webSocketUrl_;
    QWebSocket webSocket_;
    QString pendingToken_;
    qint64 pendingChannelId_ = 0;
    qint64 pendingDmThreadId_ = 0;
    /// Различает, какой из двух Hello-кадров отправить в onConnected() —
    /// pendingChannelId_/pendingDmThreadId_ сами по себе неотличимы
    /// (0 — валидный сентинел "ещё не установлен" для обоих).
    bool pendingIsDirectMessage_ = false;
};

}  // namespace devicehub
