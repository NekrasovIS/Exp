#include "WebSocketServer.h"

#include "JsonGuard.h"

#include <nlohmann/json.hpp>

#include <optional>
#include <vector>

namespace chat_service {

namespace {
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
                                                                        : nlohmann::json(nullptr)}};
}
}  // namespace

WebSocketServer::WebSocketServer(ChatService& chatService, const AuthServiceClient& authServiceClient, int port,
                                  const std::string& host)
    : chatService_(chatService), authServiceClient_(authServiceClient), server_(port, host) {
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
            {
                const std::lock_guard<std::mutex> lock(subscriptionsMutex_);
                if (const auto it = subscriptions_.find(&webSocket); it != subscriptions_.end()) {
                    subscription = it->second;
                    subscriptions_.erase(it);
                }
            }
            if (subscription.has_value()) {
                removeCallParticipant(*subscription, &webSocket);
            }
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
    if (body.is_discarded() || !body.contains("token") || !body["token"].is_string() ||
        !body.contains("channel_id") || !body["channel_id"].is_number_integer()) {
        webSocket.send(nlohmann::json{{"error", "expected {\"token\", \"channel_id\"}"}}.dump());
        webSocket.close();
        return;
    }

    const std::optional<std::string> login = authServiceClient_.verifyToken(body["token"].get<std::string>());
    if (!login.has_value()) {
        webSocket.send(nlohmann::json{{"error", "invalid token"}}.dump());
        webSocket.close();
        return;
    }

    const auto channelId = body["channel_id"].get<std::int64_t>();
    {
        const std::lock_guard<std::mutex> lock(subscriptionsMutex_);
        subscriptions_[&webSocket] = Subscription{.login = *login, .channelId = channelId};
    }
    webSocket.send(nlohmann::json{{"subscribed", true}, {"channel_id", channelId}}.dump());
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

    const std::optional<Message> stored = chatService_.postMessage(
        subscription.channelId, subscription.login, body["body"].get<std::string>(), attachmentId);
    if (!stored.has_value()) {
        webSocket.send(nlohmann::json{{"error", "no such channel, or no such attachment"}}.dump());
        return;
    }

    broadcastToChannel(subscription.channelId, toJson(*stored).dump());
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
    nlohmann::json roster = nlohmann::json::array();
    {
        const std::lock_guard<std::mutex> lock(subscriptionsMutex_);
        auto& participants = callParticipants_[subscription.channelId];
        for (const auto& entry : participants) {
            roster.push_back(entry.first);
        }
        participants[subscription.login] = &webSocket;
    }

    webSocket.send(nlohmann::json{{"call_roster", roster}}.dump());
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

void WebSocketServer::handleTyping(ix::WebSocket& webSocket, const Subscription& subscription) {
    broadcastToChannel(subscription.channelId, nlohmann::json{{"user_typing", subscription.login}}.dump(),
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

}  // namespace chat_service
