#include "WebSocketServer.h"

#include <nlohmann/json.hpp>

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
                handleChatMessage(webSocket, message->str);
            } else {
                handleHello(webSocket, message->str);
            }
            break;
        }
        case ix::WebSocketMessageType::Close:
        case ix::WebSocketMessageType::Error: {
            const std::lock_guard<std::mutex> lock(subscriptionsMutex_);
            subscriptions_.erase(&webSocket);
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

void WebSocketServer::handleChatMessage(ix::WebSocket& webSocket, const std::string& payload) {
    Subscription subscription;
    {
        const std::lock_guard<std::mutex> lock(subscriptionsMutex_);
        subscription = subscriptions_.at(&webSocket);
    }

    const nlohmann::json body = nlohmann::json::parse(payload, nullptr, /*allow_exceptions=*/false);
    if (body.is_discarded() || !body.contains("body") || !body["body"].is_string()) {
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

void WebSocketServer::broadcastToChannel(std::int64_t channelId, const std::string& json) {
    const std::lock_guard<std::mutex> lock(subscriptionsMutex_);
    for (const std::shared_ptr<ix::WebSocket>& client : server_.getClients()) {
        const auto it = subscriptions_.find(client.get());
        if (it != subscriptions_.end() && it->second.channelId == channelId) {
            client->send(json);
        }
    }
}

}  // namespace chat_service
