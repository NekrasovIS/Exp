#include "WebSocketServer.h"

#include "JsonGuard.h"

#include <nlohmann/json.hpp>

#include <chrono>
#include <optional>
#include <set>
#include <thread>
#include <vector>

namespace chat_service {

namespace {
/// Issue #226 (pentest) — см. doc-комментарий у проверки в handleCallSignal().
constexpr std::size_t kMaxCallSignalPayloadBytes = 64 * 1024;
/// Issue #312 — щедрый предел на длину эмодзи (в code units UTF-16 —
/// nlohmann::json хранит std::string/UTF-8, но лимит всё равно считает
/// байты UTF-8, тот же порядок величины): реальные эмодзи вплоть до
/// составных ZWJ-последовательностей (например, семья из нескольких
/// человек с модификаторами тона кожи) — единицы-десятки байт, не
/// сотни; предел только чтобы отказать явно неэмодзи-строке, а не
/// точно проверять, что это ровно один codepoint/grapheme.
constexpr std::size_t kMaxCallReactionEmojiBytes = 64;

nlohmann::json toJson(const Message& message) {
    return nlohmann::json{
        {"id", message.id},
        {"author", message.authorLogin},
        {"body", message.body},
        {"sent_at", message.sentAt},
        {"edited_at", message.editedAt.has_value() ? nlohmann::json(*message.editedAt) : nlohmann::json(nullptr)},
        {"attachment_id",
         message.attachmentId.has_value() ? nlohmann::json(*message.attachmentId) : nlohmann::json(nullptr)},
        {"attachment_filename", message.attachmentFilename.has_value() ? nlohmann::json(*message.attachmentFilename)
                                                                        : nlohmann::json(nullptr)},
        {"reply_to_message_id", message.replyToMessageId.has_value() ? nlohmann::json(*message.replyToMessageId)
                                                                       : nlohmann::json(nullptr)}};
}

nlohmann::json toJson(const DirectMessage& message) {
    return nlohmann::json{
        {"id", message.id}, {"author", message.authorLogin}, {"body", message.body}, {"sent_at", message.sentAt}};
}
}  // namespace

WebSocketServer::WebSocketServer(ChatService& chatService, const AuthServiceClient& authServiceClient,
                                  const JanusClient& janusClient, int port, const std::string& host)
    : chatService_(chatService), authServiceClient_(authServiceClient), janusClient_(janusClient), server_(port, host) {
    server_.setOnClientMessageCallback(
        [this](const std::shared_ptr<ix::ConnectionState>& connectionState, ix::WebSocket& webSocket,
               const ix::WebSocketMessagePtr& message) { handleMessage(connectionState, webSocket, message); });
}

bool WebSocketServer::start() {
    return server_.listenAndStart();
}

void WebSocketServer::stop() {
    server_.stop();
}

void WebSocketServer::handleMessage(const std::shared_ptr<ix::ConnectionState>& /*connectionState*/,
                                     ix::WebSocket& webSocket, const ix::WebSocketMessagePtr& message) {
    switch (message->type) {
        case ix::WebSocketMessageType::Message: {
            bool subscribed = false;
            {
                const std::lock_guard<std::mutex> lock(subscriptionsMutex_);
                subscribed = subscriptions_.contains(&webSocket);
            }
            if (subscribed) {
                handleSubscribedMessage(webSocket, message->str);
            } else {
                handleHello(webSocket, message->str);
            }
            break;
        }
        case ix::WebSocketMessageType::Close:
        case ix::WebSocketMessageType::Error: {
            std::optional<Subscription> subscription;
            // Issue #309 — whether this login has any other subscription
            // left in the same community (a second tab) determines
            // whether to actually broadcast "offline": checked in the
            // same locked block as the erase, right after it, so the
            // snapshot reflects subscriptions_ with this socket already
            // removed.
            bool stillOnlineInCommunity = false;
            {
                const std::lock_guard<std::mutex> lock(subscriptionsMutex_);
                if (const auto it = subscriptions_.find(&webSocket); it != subscriptions_.end()) {
                    subscription = it->second;
                    subscriptions_.erase(it);
                }
                if (subscription.has_value() && !subscription->isDirectMessage) {
                    for (const auto& [socket, existing] : subscriptions_) {
                        if (!existing.isDirectMessage && existing.communityId == subscription->communityId &&
                            existing.login == subscription->login) {
                            stillOnlineInCommunity = true;
                            break;
                        }
                    }
                }
            }
            if (subscription.has_value()) {
                removeCallParticipant(*subscription, &webSocket);
                if (!subscription->isDirectMessage && !stillOnlineInCommunity) {
                    broadcastToCommunity(
                        subscription->communityId,
                        nlohmann::json{{"presence_changed", {{"login", subscription->login}, {"online", false}}}}
                            .dump(),
                        &webSocket);
                }
            }
            stopJanusProxySession(&webSocket);
            break;
        }
        default:
            break;
    }
}

void WebSocketServer::handleHello(ix::WebSocket& webSocket, const std::string& payload) {
    if (json_guard::exceedsMaxNestingDepth(payload, json_guard::kMaxNestingDepth)) {
        webSocket.send(nlohmann::json{{"error", "payload too deeply nested"}}.dump());
        webSocket.close();
        return;
    }
    const nlohmann::json body = nlohmann::json::parse(payload, nullptr, /*allow_exceptions=*/false);
    const bool hasChannelId = body.contains("channel_id") && body["channel_id"].is_number_integer();
    const bool hasDmThreadId = body.contains("dm_thread_id") && body["dm_thread_id"].is_number_integer();
    if (body.is_discarded() || !body.contains("token") || !body["token"].is_string() ||
        (!hasChannelId && !hasDmThreadId)) {
        webSocket.send(
            nlohmann::json{{"error", "expected {\"token\", \"channel_id\"} or {\"token\", \"dm_thread_id\"}"}}
                .dump());
        webSocket.close();
        return;
    }

    const std::optional<std::string> login = authServiceClient_.verifyToken(body["token"].get<std::string>());
    if (!login.has_value()) {
        webSocket.send(nlohmann::json{{"error", "invalid token"}}.dump());
        webSocket.close();
        return;
    }

    if (hasDmThreadId) {
        const auto dmThreadId = body["dm_thread_id"].get<std::int64_t>();
        // 404, а не 403, для не-участника — та же приватность, что и у
        // HttpServer::handlePostDirectMessage()/handleListDirectMessages():
        // не подтверждать чужому существование диалога.
        if (!chatService_.isThreadParticipant(dmThreadId, *login)) {
            webSocket.send(nlohmann::json{{"error", "no such thread"}}.dump());
            webSocket.close();
            return;
        }
        {
            const std::lock_guard<std::mutex> lock(subscriptionsMutex_);
            subscriptions_[&webSocket] =
                Subscription{.login = *login, .isDirectMessage = true, .dmThreadId = dmThreadId};
        }
        webSocket.send(nlohmann::json{{"subscribed", true}, {"dm_thread_id", dmThreadId}}.dump());
        return;
    }

    const auto channelId = body["channel_id"].get<std::int64_t>();
    // Issue #256 (pentest): раньше подписка на channel_id не проверяла
    // членство в сообществе-владельце вообще — только валидность
    // токена, в отличие от dm_thread_id выше (isThreadParticipant()).
    // Любой аутентифицированный пользователь мог подписаться на живой
    // поток сообщений/typing/edit/delete/call-сигналинга чужого канала.
    // Тот же 404-стиль ответа и здесь — не подтверждаем существование
    // канала не-участнику.
    const std::optional<Channel> channel = chatService_.findChannel(channelId);
    if (!channel.has_value() || !chatService_.isMember(channel->communityId, *login)) {
        webSocket.send(nlohmann::json{{"error", "no such channel"}}.dump());
        webSocket.close();
        return;
    }
    // Issue #309 — collected under the same lock as the insert below, so
    // the snapshot is consistent with what other threads could observe
    // concurrently. A std::set, not vector: the same login can already
    // have another subscription open on a different channel of this
    // community (a second tab), and should only be listed once.
    std::set<std::string> onlineMembers;
    {
        const std::lock_guard<std::mutex> lock(subscriptionsMutex_);
        for (const auto& [socket, existing] : subscriptions_) {
            if (!existing.isDirectMessage && existing.communityId == channel->communityId &&
                existing.login != *login) {
                onlineMembers.insert(existing.login);
            }
        }
        subscriptions_[&webSocket] =
            Subscription{.login = *login, .channelId = channelId, .communityId = channel->communityId};
    }
    webSocket.send(nlohmann::json{{"subscribed", true},
                                   {"channel_id", channelId},
                                   {"online_members", onlineMembers}}
                       .dump());
    broadcastToCommunity(channel->communityId,
                          nlohmann::json{{"presence_changed", {{"login", *login}, {"online", true}}}}.dump(),
                          &webSocket);
}

void WebSocketServer::handleSubscribedMessage(ix::WebSocket& webSocket, const std::string& payload) {
    Subscription subscription;
    {
        const std::lock_guard<std::mutex> lock(subscriptionsMutex_);
        subscription = subscriptions_.at(&webSocket);
    }

    if (json_guard::exceedsMaxNestingDepth(payload, json_guard::kMaxNestingDepth)) {
        webSocket.send(nlohmann::json{{"error", "payload too deeply nested"}}.dump());
        return;
    }
    const nlohmann::json body = nlohmann::json::parse(payload, nullptr, /*allow_exceptions=*/false);
    if (body.is_discarded()) {
        webSocket.send(nlohmann::json{{"error", "malformed JSON"}}.dump());
        return;
    }

    // Личные диалоги не проходят общую диспетчеризацию ниже — звонки/
    // typing/edit/delete не поддерживаются для них на этом этапе (см.
    // doc-комментарий класса), поэтому единственный валидный кадр —
    // {"body"}.
    if (subscription.isDirectMessage) {
        handleDirectMessage(webSocket, subscription, body);
        return;
    }

    // Ключи сигналинга звонка проверяются первыми, тот же стиль
    // диспетчеризации по наличию ключа, что и в остальной части этого
    // протокола (см. ChatClient::onTextMessageReceived на стороне
    // DeviceHub) — при отсутствии совпадения проваливается в путь
    // сообщения чата (ожидает {"body"}).
    if (body.contains("call_join")) {
        handleCallJoin(webSocket, subscription);
    } else if (body.contains("call_leave")) {
        handleCallLeave(webSocket, subscription);
    } else if (body.contains("call_signal")) {
        handleCallSignal(webSocket, subscription, body["call_signal"]);
    } else if (body.contains("call_reaction")) {
        handleCallReaction(webSocket, subscription, body["call_reaction"]);
    } else if (body.contains("janus_attach")) {
        handleJanusAttach(webSocket);
    } else if (body.contains("janus_message")) {
        handleJanusMessage(webSocket, subscription, body["janus_message"]);
    } else if (body.contains("typing")) {
        handleTyping(webSocket, subscription);
    } else if (body.contains("edit_message")) {
        handleEditMessage(webSocket, subscription, body["edit_message"]);
    } else if (body.contains("delete_message")) {
        handleDeleteMessage(webSocket, subscription, body["delete_message"]);
    } else {
        handleChatMessage(webSocket, subscription, body);
    }
}

void WebSocketServer::handleChatMessage(ix::WebSocket& webSocket, const Subscription& subscription,
                                         const nlohmann::json& body) {
    if (!body.contains("body") || !body["body"].is_string()) {
        webSocket.send(nlohmann::json{{"error", "expected {\"body\"}"}}.dump());
        return;
    }

    // "attachment_id" (issue #116) опционален — само вложение
    // загружается отдельно через REST (HttpServer::handleUploadAttachment())
    // заранее; этот кадр только ссылается на него по id. Нет проверки
    // принадлежности, что id относится к этому отправителю/каналу — тот
    // же уровень доверия, что и в остальной части этого протокола
    // (действительный токен — единственный барьер).
    const std::optional<std::int64_t> attachmentId =
        (body.contains("attachment_id") && body["attachment_id"].is_number_integer())
            ? std::make_optional(body["attachment_id"].get<std::int64_t>())
            : std::nullopt;
    // "reply_to_message_id" (issue #306) — тоже опционален, тот же
    // уровень доверия, что и у attachment_id выше: не проверяется, что
    // сообщение с этим id вообще существует в этом канале (нет FK на
    // уровне схемы, см. её doc-комментарий в init.sql) — клиент сам
    // решает, что показать, если не найдёт его в своей истории.
    const std::optional<std::int64_t> replyToMessageId =
        (body.contains("reply_to_message_id") && body["reply_to_message_id"].is_number_integer())
            ? std::make_optional(body["reply_to_message_id"].get<std::int64_t>())
            : std::nullopt;

    const std::optional<Message> stored =
        chatService_.postMessage(subscription.channelId, subscription.login, body["body"].get<std::string>(),
                                  attachmentId, replyToMessageId);
    if (!stored.has_value()) {
        webSocket.send(nlohmann::json{{"error", "no such channel, or no such attachment"}}.dump());
        return;
    }

    broadcastToChannel(subscription.channelId, toJson(*stored).dump());
}

void WebSocketServer::handleDirectMessage(ix::WebSocket& webSocket, const Subscription& subscription,
                                           const nlohmann::json& body) {
    // issue #313: checked first, same dispatch style as
    // handleSubscribedMessage()'s own key-presence chain for channels —
    // falls through to the {"body"} path below when absent.
    if (body.contains("typing")) {
        handleDmTyping(webSocket, subscription);
        return;
    }

    if (!body.contains("body") || !body["body"].is_string()) {
        webSocket.send(nlohmann::json{{"error", "expected {\"body\"}"}}.dump());
        return;
    }

    const std::optional<DirectMessage> stored =
        chatService_.postDirectMessage(subscription.dmThreadId, subscription.login, body["body"].get<std::string>());
    if (!stored.has_value()) {
        webSocket.send(nlohmann::json{{"error", "no such thread"}}.dump());
        return;
    }

    broadcastToDmThread(subscription.dmThreadId, toJson(*stored).dump());
}

namespace {
/// Используется совместно handleEditMessage()/handleDeleteMessage():
/// kNotFound и kForbidden оба отправляются только обратно отправителю,
/// никогда не рассылаются — неудачная попытка редактирования/удаления
/// не то, о чём нужно знать другим подписчикам. @p forbiddenMessage
/// различается у двух вызывающих сторон (issue #114 расширил круг тех,
/// кто может удалять, но не тех, кто может редактировать — см.
/// doc-комментарий ChatRepository::editMessage()).
bool respondIfMutationFailed(ix::WebSocket& webSocket, MutationResult result, const char* forbiddenMessage) {
    switch (result) {
        case MutationResult::kNotFound:
            webSocket.send(nlohmann::json{{"error", "no such message"}}.dump());
            return true;
        case MutationResult::kForbidden:
            webSocket.send(nlohmann::json{{"error", forbiddenMessage}}.dump());
            return true;
        case MutationResult::kConflict:
        case MutationResult::kSuccess:
            return false;
    }
    return false;
}
}  // namespace

void WebSocketServer::handleEditMessage(ix::WebSocket& webSocket, const Subscription& subscription,
                                         const nlohmann::json& body) {
    if (!body.contains("id") || !body["id"].is_number_integer() || !body.contains("body") ||
        !body["body"].is_string()) {
        webSocket.send(nlohmann::json{{"error", "expected {\"id\", \"body\"}"}}.dump());
        return;
    }

    const auto messageId = body["id"].get<std::int64_t>();
    const EditMessageResult result = chatService_.editMessage(messageId, subscription.channelId, subscription.login,
                                                                body["body"].get<std::string>());
    if (respondIfMutationFailed(webSocket, result.result, "only the message's author may do that")) {
        return;
    }

    broadcastToChannel(subscription.channelId,
                        nlohmann::json{{"message_edited",
                                        {{"id", messageId},
                                         {"body", body["body"].get<std::string>()},
                                         {"edited_at", result.editedAt}}}}
                            .dump());
}

void WebSocketServer::handleDeleteMessage(ix::WebSocket& webSocket, const Subscription& subscription,
                                           const nlohmann::json& body) {
    if (!body.contains("id") || !body["id"].is_number_integer()) {
        webSocket.send(nlohmann::json{{"error", "expected {\"id\"}"}}.dump());
        return;
    }

    const auto messageId = body["id"].get<std::int64_t>();
    const MutationResult result = chatService_.deleteMessage(messageId, subscription.channelId, subscription.login);
    if (respondIfMutationFailed(webSocket, result,
                                 "only the message's author, the channel/community owner, or a moderator may do that")) {
        return;
    }

    broadcastToChannel(subscription.channelId, nlohmann::json{{"message_deleted", {{"id", messageId}}}}.dump());
}

void WebSocketServer::handleCallJoin(ix::WebSocket& webSocket, const Subscription& subscription) {
    // Issue #231: раньше call_join не проверял членство в канале вообще —
    // любой обладатель валидного токена мог присоединиться к звонку любого
    // канала. Переиспользуем ту же проверку, что и у остального доступа к
    // каналу, вместо отдельной системы прав только для звонков.
    if (!chatService_.isChannelMember(subscription.channelId, subscription.login)) {
        webSocket.send(nlohmann::json{{"error", "not a member of this channel"}}.dump());
        return;
    }

    // SFU-комната (issue #123/#230/#231): id детерминированно вычисляется
    // из channelId, поэтому повторный call_join на тот же канал не плодит
    // новые videoroom — ensureRoomExists() сама идемпотентна на стороне
    // Janus (проверяет "exists" перед "create"). Недоступность Janus не
    // должна ронять mesh-присутствие ниже (оно от SFU не зависит, пока
    // существуют оба пути — issue #232/#233 всё это переключат/уберут) —
    // отсутствие "sfu_room" в ответе означает "SFU для этого звонка сейчас
    // недоступен", клиент, ещё не умеющий его использовать (issue #232),
    // это поле просто игнорирует.
    const std::string janusRoomId = "channel-" + std::to_string(subscription.channelId);
    std::optional<std::string> sfuRoom;
    if (janusClient_.ensureRoomExists(janusRoomId)) {
        chatService_.recordCallRoom(subscription.channelId, janusRoomId);
        sfuRoom = janusRoomId;
    }

    nlohmann::json roster = nlohmann::json::array();
    {
        const std::lock_guard<std::mutex> lock(subscriptionsMutex_);
        auto& participants = callParticipants_[subscription.channelId];
        for (const auto& entry : participants) {
            roster.push_back(entry.first);
        }
        participants[subscription.login] = &webSocket;
    }

    webSocket.send(nlohmann::json{{"call_roster", roster},
                                   {"sfu_room", sfuRoom.has_value() ? nlohmann::json(*sfuRoom) : nlohmann::json(nullptr)}}
                       .dump());
    broadcastToCallParticipants(subscription.channelId, nlohmann::json{{"call_peer_joined", subscription.login}}.dump(),
                                 &webSocket);
}

void WebSocketServer::handleCallLeave(ix::WebSocket& webSocket, const Subscription& subscription) {
    removeCallParticipant(subscription, &webSocket);
}

void WebSocketServer::handleCallSignal(ix::WebSocket& webSocket, const Subscription& subscription,
                                        const nlohmann::json& body) {
    if (!body.contains("to") || !body["to"].is_string() || !body.contains("payload")) {
        webSocket.send(nlohmann::json{{"error", "expected {\"to\", \"payload\"}"}}.dump());
        return;
    }
    // Issue #226 (pentest): "payload" — непрозрачные данные (SDP/ICE),
    // ретранслируемые целевому пиру без разбора; json_guard's проверка
    // глубины вложенности выше в handleSubscribedMessage() не спасает
    // от одной очень длинной строки (глубина 1) — нужен отдельный
    // предел на сериализованный размер. Реальные SDP/ICE-кандидаты —
    // единицы-десятки КБ, 64 КиБ — запас с большим запасом, не с запасом
    // впритык.
    if (body["payload"].dump().size() > kMaxCallSignalPayloadBytes) {
        webSocket.send(nlohmann::json{{"error", "'payload' exceeds the size limit"}}.dump());
        return;
    }

    ix::WebSocket* target = nullptr;
    {
        const std::lock_guard<std::mutex> lock(subscriptionsMutex_);
        const auto channelIt = callParticipants_.find(subscription.channelId);
        if (channelIt != callParticipants_.end()) {
            const auto peerIt = channelIt->second.find(body["to"].get<std::string>());
            if (peerIt != channelIt->second.end()) {
                target = peerIt->second;
            }
        }
    }

    if (target == nullptr) {
        webSocket.send(nlohmann::json{{"error", "peer not in call"}}.dump());
        return;
    }

    target->send(
        nlohmann::json{{"call_signal", {{"from", subscription.login}, {"payload", body["payload"]}}}}.dump());
}

void WebSocketServer::handleCallReaction(ix::WebSocket& webSocket, const Subscription& subscription,
                                          const nlohmann::json& emoji) {
    if (!emoji.is_string() || emoji.get_ref<const std::string&>().empty() ||
        emoji.get_ref<const std::string&>().size() > kMaxCallReactionEmojiBytes) {
        webSocket.send(nlohmann::json{{"error", "expected {\"call_reaction\": \"<emoji>\"}"}}.dump());
        return;
    }

    // Отправитель сам должен сейчас быть участником звонка этого канала
    // — та же проверка присутствия, что и у handleCallSignal() выше,
    // только на себя, а не на целевого пира.
    bool senderInCall = false;
    {
        const std::lock_guard<std::mutex> lock(subscriptionsMutex_);
        const auto channelIt = callParticipants_.find(subscription.channelId);
        if (channelIt != callParticipants_.end()) {
            const auto selfIt = channelIt->second.find(subscription.login);
            senderInCall = selfIt != channelIt->second.end() && selfIt->second == &webSocket;
        }
    }
    if (!senderInCall) {
        webSocket.send(nlohmann::json{{"error", "not in a call"}}.dump());
        return;
    }

    broadcastToCallParticipants(
        subscription.channelId,
        nlohmann::json{{"call_reaction", {{"login", subscription.login}, {"emoji", emoji}}}}.dump(), &webSocket);
}

void WebSocketServer::handleJanusAttach(ix::WebSocket& webSocket) {
    std::int64_t sessionId = 0;
    bool haveSession = false;
    {
        const std::lock_guard<std::mutex> lock(janusSessionsMutex_);
        if (const auto it = janusSessions_.find(&webSocket); it != janusSessions_.end()) {
            sessionId = it->second.sessionId;
            haveSession = true;
        }
    }

    if (!haveSession) {
        const std::optional<std::int64_t> newSessionId = janusClient_.createSession();
        if (!newSessionId) {
            webSocket.send(nlohmann::json{{"error", "SFU unavailable"}}.dump());
            return;
        }
        sessionId = *newSessionId;

        // pumpJanusEvents() переживёт возврат из этого обработчика и
        // должно продолжать слать в этот же сокет из отдельного потока —
        // нужен shared_ptr, не сырой &webSocket (тот же приём, что и у
        // adресатов в broadcastToChannel()).
        std::shared_ptr<ix::WebSocket> socketHandle;
        for (const std::shared_ptr<ix::WebSocket>& client : server_.getClients()) {
            if (client.get() == &webSocket) {
                socketHandle = client;
                break;
            }
        }
        if (!socketHandle) {
            webSocket.send(nlohmann::json{{"error", "connection not found"}}.dump());
            return;
        }

        JanusProxySession session;
        session.sessionId = sessionId;
        session.eventPump = std::jthread([this, socketHandle, sessionId](const std::stop_token& stopToken) {
            pumpJanusEvents(socketHandle, sessionId, stopToken);
        });
        const std::lock_guard<std::mutex> lock(janusSessionsMutex_);
        janusSessions_[&webSocket] = std::move(session);
    }

    const std::optional<std::int64_t> handleId = janusClient_.attachHandle(sessionId, "janus.plugin.videoroom");
    if (!handleId) {
        webSocket.send(nlohmann::json{{"error", "failed to attach Janus handle"}}.dump());
        return;
    }
    webSocket.send(nlohmann::json{{"janus_attached", {{"handle", *handleId}}}}.dump());
}

void WebSocketServer::handleJanusMessage(ix::WebSocket& webSocket, const Subscription& subscription,
                                          const nlohmann::json& body) {
    if (!body.contains("handle") || !body["handle"].is_number_integer() || !body.contains("body") ||
        !body["body"].is_object()) {
        webSocket.send(nlohmann::json{{"error", "expected {\"handle\", \"body\", \"jsep\"?}"}}.dump());
        return;
    }

    // Единственный барьер против использования через этот прокси чужой
    // комнаты (issue #232): если body содержит "room", он обязан быть
    // комнатой именно этого канала — та же формула, что и в
    // handleCallJoin(). Без этой проверки клиент мог бы просто подставить
    // room другого канала и обойти проверку членства из call_join.
    if (body["body"].contains("room")) {
        const std::string expectedRoom = "channel-" + std::to_string(subscription.channelId);
        if (!body["body"]["room"].is_string() || body["body"]["room"].get<std::string>() != expectedRoom) {
            webSocket.send(nlohmann::json{{"error", "room mismatch"}}.dump());
            return;
        }
    }

    std::int64_t sessionId = 0;
    {
        const std::lock_guard<std::mutex> lock(janusSessionsMutex_);
        const auto it = janusSessions_.find(&webSocket);
        if (it == janusSessions_.end()) {
            webSocket.send(nlohmann::json{{"error", "no Janus session — send janus_attach first"}}.dump());
            return;
        }
        sessionId = it->second.sessionId;
    }

    const auto handleId = body["handle"].get<std::int64_t>();
    const std::optional<nlohmann::json> jsep = body.contains("jsep") ? std::make_optional(body["jsep"]) : std::nullopt;
    const std::optional<nlohmann::json> response = janusClient_.sendMessage(sessionId, handleId, body["body"], jsep);
    if (!response) {
        webSocket.send(nlohmann::json{{"error", "SFU unavailable"}}.dump());
        return;
    }
    webSocket.send(nlohmann::json{{"janus_message_ack", *response}}.dump());
}

void WebSocketServer::pumpJanusEvents(std::shared_ptr<ix::WebSocket> socket, std::int64_t sessionId,
                                       std::stop_token stopToken) {
    while (!stopToken.stop_requested()) {
        const std::optional<nlohmann::json> event = janusClient_.longPollOnce(sessionId);
        if (stopToken.stop_requested()) {
            return;
        }
        if (!event) {
            // Janus временно недоступен — не крутиться busy-loop'ом,
            // пока остановка не запрошена или он не отвечает снова.
            std::this_thread::sleep_for(std::chrono::seconds(1));
            continue;
        }
        socket->send(nlohmann::json{{"janus_event", *event}}.dump());
    }
}

void WebSocketServer::stopJanusProxySession(ix::WebSocket* socket) {
    JanusProxySession session;
    {
        const std::lock_guard<std::mutex> lock(janusSessionsMutex_);
        const auto it = janusSessions_.find(socket);
        if (it == janusSessions_.end()) {
            return;
        }
        session = std::move(it->second);
        janusSessions_.erase(it);
    }
    // session разрушается здесь, уже вне лока — деструктор std::jthread
    // запрашивает остановку и join'ится, что может занять время вплоть до
    // тайм-аута текущего long-poll внутри pumpJanusEvents().
}

void WebSocketServer::handleTyping(ix::WebSocket& webSocket, const Subscription& subscription) {
    broadcastToChannel(subscription.channelId, nlohmann::json{{"user_typing", subscription.login}}.dump(),
                        &webSocket);
}

void WebSocketServer::handleDmTyping(ix::WebSocket& webSocket, const Subscription& subscription) {
    broadcastToDmThread(subscription.dmThreadId, nlohmann::json{{"user_typing", subscription.login}}.dump(),
                         &webSocket);
}

void WebSocketServer::removeCallParticipant(const Subscription& subscription, ix::WebSocket* socket) {
    bool wasParticipant = false;
    {
        const std::lock_guard<std::mutex> lock(subscriptionsMutex_);
        const auto channelIt = callParticipants_.find(subscription.channelId);
        if (channelIt != callParticipants_.end()) {
            const auto peerIt = channelIt->second.find(subscription.login);
            if (peerIt != channelIt->second.end() && peerIt->second == socket) {
                channelIt->second.erase(peerIt);
                wasParticipant = true;
            }
        }
    }

    if (wasParticipant) {
        broadcastToCallParticipants(subscription.channelId, nlohmann::json{{"call_peer_left", subscription.login}}.dump(),
                                     socket);
    }
}

void WebSocketServer::broadcastToChannel(std::int64_t channelId, const std::string& json,
                                          const ix::WebSocket* excludeSocket) {
    // Собираем список адресатов под локом, затем вызываем send() уже вне
    // его (CP.22: никогда не вызывать неизвестный/сторонний код —
    // ix::WebSocket::send() выполняет сетевой I/O — удерживая лок; также
    // CP.43, минимизировать время в критической секции). Копии shared_ptr
    // удерживают каждый адресат живым независимо от того, что происходит
    // с subscriptions_/собственным списком клиентов server_ между
    // освобождением лока и отправкой.
    std::vector<std::shared_ptr<ix::WebSocket>> targets;
    {
        const std::lock_guard<std::mutex> lock(subscriptionsMutex_);
        for (const std::shared_ptr<ix::WebSocket>& client : server_.getClients()) {
            if (client.get() == excludeSocket) {
                continue;
            }
            const auto it = subscriptions_.find(client.get());
            if (it != subscriptions_.end() && it->second.channelId == channelId) {
                targets.push_back(client);
            }
        }
    }
    for (const std::shared_ptr<ix::WebSocket>& client : targets) {
        client->send(json);
    }
}

void WebSocketServer::broadcastToCommunity(std::int64_t communityId, const std::string& json,
                                            const ix::WebSocket* excludeSocket) {
    // Same "collect under the lock, send outside it" shape as
    // broadcastToChannel() above (CP.22/CP.43) — the only difference is
    // matching on communityId (any channel of it) instead of one
    // specific channelId.
    std::vector<std::shared_ptr<ix::WebSocket>> targets;
    {
        const std::lock_guard<std::mutex> lock(subscriptionsMutex_);
        for (const std::shared_ptr<ix::WebSocket>& client : server_.getClients()) {
            if (client.get() == excludeSocket) {
                continue;
            }
            const auto it = subscriptions_.find(client.get());
            if (it != subscriptions_.end() && !it->second.isDirectMessage && it->second.communityId == communityId) {
                targets.push_back(client);
            }
        }
    }
    for (const std::shared_ptr<ix::WebSocket>& client : targets) {
        client->send(json);
    }
}

void WebSocketServer::broadcastToCallParticipants(std::int64_t channelId, const std::string& json,
                                                   const ix::WebSocket* excludeSocket) {
    // То же рассуждение CP.22/CP.43, что и у broadcastToChannel() выше.
    // В отличие от него, callParticipants_ хранит не владеющий
    // ix::WebSocket* (см. её doc-комментарий), а не shared_ptr, поэтому
    // это не расширяет окно действительности этого указателя сверх того,
    // что остальная часть класса уже предполагает в других местах
    // (например, removeCallParticipant() тоже оперирует сырым указателем
    // на сокет после освобождения того же лока) — лишь сокращает время,
    // в течение которого удерживается сам subscriptionsMutex_.
    std::vector<ix::WebSocket*> targets;
    {
        const std::lock_guard<std::mutex> lock(subscriptionsMutex_);
        const auto channelIt = callParticipants_.find(channelId);
        if (channelIt == callParticipants_.end()) {
            return;
        }
        for (const auto& entry : channelIt->second) {
            if (entry.second != excludeSocket) {
                targets.push_back(entry.second);
            }
        }
    }
    for (ix::WebSocket* client : targets) {
        client->send(json);
    }
}

void WebSocketServer::broadcastToDmThread(std::int64_t dmThreadId, const std::string& json,
                                           const ix::WebSocket* excludeSocket) {
    // Та же схема "собрать под локом, разослать вне его" (CP.22/CP.43),
    // что и у broadcastToChannel() — см. её doc-комментарий.
    std::vector<std::shared_ptr<ix::WebSocket>> targets;
    {
        const std::lock_guard<std::mutex> lock(subscriptionsMutex_);
        for (const std::shared_ptr<ix::WebSocket>& client : server_.getClients()) {
            if (client.get() == excludeSocket) {
                continue;
            }
            const auto it = subscriptions_.find(client.get());
            if (it != subscriptions_.end() && it->second.isDirectMessage && it->second.dmThreadId == dmThreadId) {
                targets.push_back(client);
            }
        }
    }
    for (const std::shared_ptr<ix::WebSocket>& client : targets) {
        client->send(json);
    }
}

}  // namespace chat_service
