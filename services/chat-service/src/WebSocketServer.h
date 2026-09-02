#pragma once

#include <ixwebsocket/IXWebSocketServer.h>

#include <nlohmann/json_fwd.hpp>

#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>

#include "AuthServiceClient.h"
#include "ChatService.h"

namespace chat_service {

/**
 * @brief Доставка новых сообщений в реальном времени по WebSocket, а
 *        также ретрансляция сигналинга для групповых голосовых звонков
 *        (issue #46).
 *
 * Протокол: первое сообщение, которое отправляет клиент, должно быть
 * `{"token": "...", "channel_id": N}` — проверяется через auth-service
 * (AuthServiceClient), прежде чем соединение будет подписано на этот
 * канал. Каждое следующее сообщение — одно из:
 *   - `{"body": "...", "attachment_id": N}` — сообщение чата,
 *     опционально ссылающееся на файл, уже загруженный через
 *     POST /channels/{id}/attachments из HttpServer (issue #116) —
 *     attachment_id опционален; сохраняется через ChatService, затем
 *     рассылается как JSON каждому соединению, подписанному на тот же
 *     канал (включая отправителя, чтобы все клиенты отрисовывали данные
 *     из одного и того же потока реального времени, а не делали
 *     оптимистичное локальное эхо).
 *   - `{"call_join": true}` — присоединиться к голосовому звонку для
 *     подписанного канала; отвечает `{"call_roster": [...]}`
 *     (существующие участники звонка, не сохраняются, эфемерны в
 *     пределах этого процесса) и рассылает им
 *     `{"call_peer_joined": "<login>"}`.
 *   - `{"call_leave": true}` — покинуть звонок; рассылает
 *     `{"call_peer_left": "<login>"}` оставшимся участникам.
 *     Отключение (Close/Error) без явного выхода даёт тот же эффект.
 *   - `{"call_signal": {"to": "<login>", "payload": {...}}}` —
 *     непрозрачные данные сигналинга (SDP offer/answer, ICE-кандидат),
 *     ретранслируемые дословно указанному участнику как
 *     `{"call_signal": {"from": "<login>", "payload": {...}}}` — этот
 *     класс никогда не заглядывает внутрь `payload`. Отвечает
 *     `{"error": "peer not in call"}` отправителю, если `to` не является
 *     текущим участником звонка.
 *   - `{"typing": true}` — issue #96: рассылает
 *     `{"user_typing": "<login>"}` каждому другому подписчику того же
 *     канала (никогда не отправителю обратно). Эфемерно, как и
 *     присутствие в звонке — ничего не сохраняется, нет явного
 *     сообщения "перестал печатать"; клиентская сторона сама сбрасывает
 *     индикатор по таймауту.
 *   - `{"edit_message": {"id": N, "body": "..."}}` (issue #107) —
 *     рассылает `{"message_edited": {"id", "body", "edited_at"}}`
 *     каждому подписчику *чата* (включая редактора). Редактировать
 *     сообщение может только его собственный автор, даже после введения
 *     ролей/модерации (issue #114) — см. doc-комментарий
 *     ChatRepository::editMessage() о том, почему полномочия модератора
 *     не распространяются на редактирование, только на удаление.
 *     Отвечает `{"error": ...}` отправителю (не рассылка) при 404/403.
 *   - `{"delete_message": {"id": N}}` — правило шире, чем у edit_message
 *     (issue #114): удалить сообщение может собственный автор сообщения,
 *     владелец канала/сообщества либо модератор сообщества. При успехе
 *     рассылает `{"message_deleted": {"id"}}`.
 *

 * REST (HttpServer) остаётся источником истины для истории/CRUD; этот
 * класс только проталкивает то, что отправлено, пока клиент подключён,
 * и только ретранслирует сигналинг звонков — он никогда не декодирует
 * содержимое SDP/ICE.
 */
class WebSocketServer {
public:
    WebSocketServer(ChatService& chatService, const AuthServiceClient& authServiceClient, int port,
                     const std::string& host = "127.0.0.1");

    /// Начинает принимать соединения; возвращает управление после начала прослушивания (дальше асинхронно).
    bool start();

    /// Прекращает принимать соединения и закрывает существующие.
    void stop();

private:
    struct Subscription {
        std::string login;
        std::int64_t channelId = 0;
    };

    void handleMessage(const std::shared_ptr<ix::ConnectionState>& connectionState, ix::WebSocket& webSocket,
                        const ix::WebSocketMessagePtr& message);
    void handleHello(ix::WebSocket& webSocket, const std::string& payload);
    void handleSubscribedMessage(ix::WebSocket& webSocket, const std::string& payload);
    void handleChatMessage(ix::WebSocket& webSocket, const Subscription& subscription, const nlohmann::json& body);
    void handleEditMessage(ix::WebSocket& webSocket, const Subscription& subscription, const nlohmann::json& body);
    void handleDeleteMessage(ix::WebSocket& webSocket, const Subscription& subscription, const nlohmann::json& body);
    void handleCallJoin(ix::WebSocket& webSocket, const Subscription& subscription);
    void handleCallLeave(ix::WebSocket& webSocket, const Subscription& subscription);
    void handleCallSignal(ix::WebSocket& webSocket, const Subscription& subscription, const nlohmann::json& body);
    void handleTyping(ix::WebSocket& webSocket, const Subscription& subscription);
    void removeCallParticipant(const Subscription& subscription, ix::WebSocket* socket);
    /// Отправляет @p json каждому сокету, подписанному на чат @p channelId,
    /// кроме @p excludeSocket (nullptr — значение по умолчанию — не
    /// исключает никого, что и нужно обычной рассылке сообщения чата;
    /// уведомления о наборе текста передают сюда отправителя, чтобы он
    /// не видел эхо собственного "typing").
    void broadcastToChannel(std::int64_t channelId, const std::string& json, const ix::WebSocket* excludeSocket = nullptr);
    /// В отличие от broadcastToChannel (все подписчики *чата* канала),
    /// это достигает только сокетов, реально находящихся в
    /// callParticipants_[channelId] — тот, кто подписан на текстовый чат
    /// канала, но не в звонке, не должен видеть трафик присутствия в
    /// звонке. Никогда не отправляет @p excludeSocket (как правило,
    /// участнику, который только что вызвал уведомление).
    void broadcastToCallParticipants(std::int64_t channelId, const std::string& json,
                                      const ix::WebSocket* excludeSocket);

    ChatService& chatService_;
    const AuthServiceClient& authServiceClient_;
    ix::WebSocketServer server_;

    std::mutex subscriptionsMutex_;
    std::unordered_map<ix::WebSocket*, Subscription> subscriptions_;
    // channelId -> login -> socket. Эфемерное присутствие в звонке,
    // отдельно от подписки на *чат* канала выше — клиент может быть
    // подписан на текстовый чат канала, не будучи в его звонке.
    std::unordered_map<std::int64_t, std::unordered_map<std::string, ix::WebSocket*>> callParticipants_;
};

}  // namespace chat_service
