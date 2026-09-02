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
 */
class ChatClient : public QObject {
    Q_OBJECT

public:
    explicit ChatClient(QUrl webSocketUrl, QObject* parent = nullptr);

    /// Открывает соединение и подписывается на @p channelId, используя @p token.
    void connectToChannel(const QString& token, qint64 channelId);

    /// Отправляет @p body в канал, на который подписан этот клиент,
    /// опционально ссылаясь на уже загруженный @p attachmentId
    /// (issue #116) — -1 (значение по умолчанию) означает отсутствие
    /// вложения.
    void sendMessage(const QString& body, qint64 attachmentId = -1);

    void disconnectFromChannel();

    /// Присоединяется к голосовому звонку в подписанном канале;
    /// вызывает ответ callRosterReceived().
    void joinCall();

    /// Покидает голосовой звонок в подписанном канале.
    void leaveCall();

    /// Ретранслирует непрозрачный сигналинговый @p payload (SDP
    /// offer/answer или ICE-кандидат) участнику звонка @p to.
    void sendCallSignal(const QString& to, const QJsonObject& payload);

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
    /// Испускается, как только chat-service подтверждает подписку.
    void subscribed(qint64 channelId);

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

    /// Ещё один участник присоединился к звонку.
    void callPeerJoined(const QString& login);

    /// Участник покинул звонок (явный leaveCall() или отключение).
    void callPeerLeft(const QString& login);

    /// Сигналинговый payload, ретранслированный от другого участника
    /// звонка.
    void callSignalReceived(const QString& from, const QJsonObject& payload);

    /// Другой подписчик печатает в подписанном канале.
    void userTyping(const QString& login);

private:
    void onConnected();
    void onTextMessageReceived(const QString& message);

    QUrl webSocketUrl_;
    QWebSocket webSocket_;
    QString pendingToken_;
    qint64 pendingChannelId_ = 0;
};

}  // namespace devicehub
