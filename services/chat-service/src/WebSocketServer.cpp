#include "WebSocketServer.h"

#include <nlohmann/json.hpp>

#include <optional>

namespace chat_service {

namespace {
nlohmann::json toJson(const Message& message) {
    return nlohmann::json{
        {"id", message.id}, {"author", message.authorLogin}, {"body", message.body}, {"sent_at", message.sentAt}};
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

    const nlohmann::json body = nlohmann::json::parse(payload, nullptr, /*allow_exceptions=*/false);
    if (body.is_discarded()) {
        webSocket.send(nlohmann::json{{"error", "malformed JSON"}}.dump());
        return;
    }

    // Call-signaling keys are checked first, same key-presence dispatch
    // style as the rest of this protocol (see ChatClient::onTextMessageReceived
    // on the DeviceHub side) — falls through to the chat-message path
    // (expects {"body"}) for anything that isn't one of these.
    if (body.contains("call_join")) {
        handleCallJoin(webSocket, subscription);
    } else if (body.contains("call_leave")) {
        handleCallLeave(webSocket, subscription);
    } else if (body.contains("call_signal")) {
        handleCallSignal(webSocket, subscription, body["call_signal"]);
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

    const std::optional<Message> stored =
        chatService_.postMessage(subscription.channelId, subscription.login, body["body"].get<std::string>());
    if (!stored.has_value()) {
        webSocket.send(nlohmann::json{{"error", "no such channel"}}.dump());
        return;
    }

    broadcastToChannel(subscription.channelId, toJson(*stored).dump());
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

void WebSocketServer::broadcastToChannel(std::int64_t channelId, const std::string& json) {
    const std::lock_guard<std::mutex> lock(subscriptionsMutex_);
    for (const std::shared_ptr<ix::WebSocket>& client : server_.getClients()) {
        const auto it = subscriptions_.find(client.get());
        if (it != subscriptions_.end() && it->second.channelId == channelId) {
            client->send(json);
        }
    }
}

void WebSocketServer::broadcastToCallParticipants(std::int64_t channelId, const std::string& json,
                                                   const ix::WebSocket* excludeSocket) {
    const std::lock_guard<std::mutex> lock(subscriptionsMutex_);
    const auto channelIt = callParticipants_.find(channelId);
    if (channelIt == callParticipants_.end()) {
        return;
    }
    for (const auto& entry : channelIt->second) {
        if (entry.second != excludeSocket) {
            entry.second->send(json);
        }
    }
}

}  // namespace chat_service
