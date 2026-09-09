#pragma once

#include <ixwebsocket/IXWebSocketServer.h>

#include <nlohmann/json_fwd.hpp>

#include <cstdint>
#include <memory>
#include <mutex>
#include <stop_token>
#include <string>
#include <thread>
#include <unordered_map>

#include "AuthServiceClient.h"
#include "ChatService.h"
#include "JanusClient.h"

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
 *     подписанного канала; требует членства в сообществе канала (issue
 *     #231 — раньше не проверялось вообще), иначе `{"error": "not a
 *     member of this channel"}`. Отвечает `{"call_roster": [...],
 *     "sfu_room": "channel-<id>"}` (существующий mesh-ростер участников,
 *     не сохраняется, эфемерен в пределах этого процесса; "sfu_room" —
 *     id videoroom-комнаты Janus для этого канала, идемпотентно создаётся
 *     через JanusClient при первом обращении, null, если Janus сейчас
 *     недоступен) и рассылает остальным `{"call_peer_joined": "<login>"}`.
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
 *   - `{"janus_attach": true}` (issue #232) — прокси-сигналинг SFU:
 *     attach'ит новый handle плагина videoroom к Janus-сессии этого
 *     WS-подключения (создаёт сессию при самом первом вызове; живёт до
 *     закрытия соединения, не до call_leave). Отвечает
 *     `{"janus_attached": {"handle": N}}`.
 *   - `{"janus_message": {"handle": N, "body": {...}, "jsep": {...}?}}` —
 *     пересылает `body`/`jsep` как есть указанному handle'у ("join"/
 *     "configure"/"subscribe"/"start" и т.п. — этот класс не разбирает
 *     их содержимое, кроме поля `room`, если оно есть: оно обязано
 *     совпадать с комнатой ИМЕННО этого канала — единственный барьер
 *     против использования чужой комнаты через этот прокси, см.
 *     handleJanusMessage()). Отвечает `{"janus_message_ack": {...}}` —
 *     прямым ответом Janus на сам запрос (может быть просто "ack",
 *     реальный результат/jsep-answer от Janus прилетит асинхронно как
 *     `{"janus_event": {...}}` через отдельный поток long-poll на эту
 *     сессию, запущенный в janus_attach).
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
 *   - `{"toggle_reaction": {"message_id": N, "emoji": "..."}}` (issue
 *     #333) — переключает реакцию отправителя на @p emoji: если он ещё
 *     не поставил именно эту эмодзи на это сообщение — ставит, если
 *     уже поставил — снимает (см. doc-комментарий уникального индекса
 *     message_reactions в init.sql). Доступно любому подписчику чата на
 *     любое сообщение, включая собственное — не только автору, в
 *     отличие от edit_message. Рассылает всем подписчикам чата
 *     `{"reaction_changed": {"message_id", "emoji", "logins": [...]}}`
 *     — @p logins это ПОЛНЫЙ список тех, кто сейчас поставил именно эту
 *     эмодзи на это сообщение (после применения переключения), не
 *     дельта; клиент заменяет свою локальную копию целиком, а не
 *     инкрементирует счётчик.
 *

 * REST (HttpServer) остаётся источником истины для истории/CRUD; этот
 * класс только проталкивает то, что отправлено, пока клиент подключён,
 * и только ретранслирует сигналинг звонков — он никогда не декодирует
 * содержимое SDP/ICE.
 *
 * Личные диалоги (issue #187, Фаза 2b) используют тот же протокол
 * подписки, но `{"token": "...", "dm_thread_id": N}` вместо
 * `channel_id` — вызывающая сторона должна уже быть участником диалога
 * (проверяется через ChatService::isThreadParticipant(), 404 "no such
 * thread" при отказе — та же приватность, что и у REST-эндпоинтов
 * HttpServer::handlePostDirectMessage()/handleListDirectMessages(), не
 * подтверждающих чужому существование диалога через разные коды ошибок
 * для "не найден" и "не участник"). После подписки на диалог доступно
 * только `{"body": "..."}` — звонки/typing/edit/delete не поддерживаются
 * для личных диалогов на этом этапе (backend Фазы 2 их не реализует),
 * поэтому подписка на диалог не проходит через общую диспетчеризацию
 * handleSubscribedMessage(), а сразу и только через handleDirectMessage().
 */
class WebSocketServer {
public:
    WebSocketServer(ChatService& chatService, const AuthServiceClient& authServiceClient,
                     const JanusClient& janusClient, int port, const std::string& host = "127.0.0.1");

    /// Начинает принимать соединения; возвращает управление после начала прослушивания (дальше асинхронно).
    bool start();

    /// Прекращает принимать соединения и закрывает существующие.
    void stop();

private:
    struct Subscription {
        std::string login;
        std::int64_t channelId = 0;
        /// True, когда эта подписка — на личный диалог (dmThreadId), а
        /// не на канал сообщества (channelId) — оба поля взаимно
        /// исключающие, различаются этим флагом, а не значением 0
        /// (валидные id из Postgres serial начинаются с 1, но флаг явнее
        /// сравнения с сентинелом).
        bool isDirectMessage = false;
        std::int64_t dmThreadId = 0;
    };

    /// Janus-сессия, проксируемая через одно WS-подключение (issue #232)
    /// — создаётся при первом `janus_attach`, живёт до закрытия
    /// соединения (не до `call_leave`: простая привязка ко времени жизни
    /// сокета, не к состоянию звонка — см. doc-комментарий класса).
    /// eventPump — единственный поток, делающий long-poll GET на эту
    /// sessionId (у Janus нет параллельных long-poll на одну сессию); его
    /// деструктор (std::jthread) сам запрашивает остановку и join'ится.
    struct JanusProxySession {
        std::int64_t sessionId = 0;
        std::jthread eventPump;
    };

    void handleMessage(const std::shared_ptr<ix::ConnectionState>& connectionState, ix::WebSocket& webSocket,
                        const ix::WebSocketMessagePtr& message);
    void handleHello(ix::WebSocket& webSocket, const std::string& payload);
    void handleSubscribedMessage(ix::WebSocket& webSocket, const std::string& payload);
    void handleChatMessage(ix::WebSocket& webSocket, const Subscription& subscription, const nlohmann::json& body);
    /// Личное сообщение в подписанном диалоге (issue #187, Фаза 2b) —
    /// узкий аналог handleChatMessage() для dmThreadId вместо channelId,
    /// без attachment_id (личные диалоги их не поддерживают).
    void handleDirectMessage(ix::WebSocket& webSocket, const Subscription& subscription, const nlohmann::json& body);
    void handleEditMessage(ix::WebSocket& webSocket, const Subscription& subscription, const nlohmann::json& body);
    void handleDeleteMessage(ix::WebSocket& webSocket, const Subscription& subscription, const nlohmann::json& body);
    /// {"toggle_reaction": {"message_id", "emoji"}} (issue #333) — см.
    /// doc-комментарий класса.
    void handleToggleReaction(ix::WebSocket& webSocket, const Subscription& subscription, const nlohmann::json& body);
    void handleCallJoin(ix::WebSocket& webSocket, const Subscription& subscription);
    void handleCallLeave(ix::WebSocket& webSocket, const Subscription& subscription);
    void handleCallSignal(ix::WebSocket& webSocket, const Subscription& subscription, const nlohmann::json& body);
    /// issue #232: attach новый videoroom-handle на Janus-сессию этого
    /// подключения, создавая саму сессию (и запуская pumpJanusEvents())
    /// при первом обращении.
    void handleJanusAttach(ix::WebSocket& webSocket);
    /// issue #232: пересылает {"handle", "body", "jsep"?} указанному
    /// handle'у Janus-сессии этого подключения — см. doc-комментарий
    /// класса о проверке поля "room".
    void handleJanusMessage(ix::WebSocket& webSocket, const Subscription& subscription, const nlohmann::json& body);
    /// Тело фонового потока JanusProxySession::eventPump — один
    /// блокирующий long-poll на @p sessionId за раз, пока не запрошена
    /// остановка; каждое полученное событие уходит в @p socket как
    /// `{"janus_event": ...}`. См. doc-комментарий класса о верхней
    /// границе задержки остановки.
    void pumpJanusEvents(std::shared_ptr<ix::WebSocket> socket, std::int64_t sessionId, std::stop_token stopToken);
    /// Останавливает (join'ит) и удаляет Janus-прокси-сессию @p socket,
    /// если она есть — вызывается при закрытии WS-соединения. Извлекает
    /// сессию из карты под локом, но join (потенциально небыстрый, см.
    /// pumpJanusEvents()) происходит уже вне его — тот же принцип CP.43,
    /// что и у broadcastToChannel()/broadcastToCallParticipants().
    void stopJanusProxySession(ix::WebSocket* socket);
    void handleTyping(ix::WebSocket& webSocket, const Subscription& subscription);
    void removeCallParticipant(const Subscription& subscription, ix::WebSocket* socket);
    /// Отправляет @p json каждому сокету, подписанному на чат @p channelId,
    /// кроме @p excludeSocket (nullptr — значение по умолчанию — не
    /// исключает никого, что и нужно обычной рассылке сообщения чата;
    /// уведомления о наборе текста передают сюда отправителя, чтобы он
    /// не видел эхо собственного "typing").
    void broadcastToChannel(std::int64_t channelId, const std::string& json, const ix::WebSocket* excludeSocket = nullptr);
    /// Аналог broadcastToChannel() для подписчиков личного диалога
    /// dmThreadId — их всегда ровно два (участники), включая
    /// отправителя (тот же принцип "рассылка всем, без локального
    /// оптимистичного эха", что и у broadcastToChannel()).
    void broadcastToDmThread(std::int64_t dmThreadId, const std::string& json);
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
    const JanusClient& janusClient_;
    ix::WebSocketServer server_;

    std::mutex subscriptionsMutex_;
    std::unordered_map<ix::WebSocket*, Subscription> subscriptions_;
    // channelId -> login -> socket. Эфемерное присутствие в звонке,
    // отдельно от подписки на *чат* канала выше — клиент может быть
    // подписан на текстовый чат канала, не будучи в его звонке.
    std::unordered_map<std::int64_t, std::unordered_map<std::string, ix::WebSocket*>> callParticipants_;

    // Отдельный мьютекс, не subscriptionsMutex_ — janusSessions_ живёт по
    // своему собственному циклу (привязан к сокету, не к call_join/leave)
    // и его собственная операция (join фонового потока в
    // stopJanusProxySession()) не должна удерживать лок, под которым
    // рассылки и call-присутствие ждут своей очереди.
    std::mutex janusSessionsMutex_;
    std::unordered_map<ix::WebSocket*, JanusProxySession> janusSessions_;
};

}  // namespace chat_service
