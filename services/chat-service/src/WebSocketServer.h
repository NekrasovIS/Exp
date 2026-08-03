#pragma once

#include <ixwebsocket/IXWebSocketServer.h>

#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>

#include "AuthServiceClient.h"
#include "ChatService.h"

namespace chat_service {

/**
 * @brief Real-time delivery of new messages over WebSocket.
 *
 * Protocol: the first message a client sends must be
 * `{"token": "...", "channel_id": N}` — verified against auth-service
 * (AuthServiceClient) before the connection is subscribed to that
 * channel. Every message after that is `{"body": "..."}`: persisted via
 * ChatService, then broadcast as JSON to every connection subscribed to
 * the same channel (including the sender, so all clients render off the
 * same real-time stream rather than optimistically local-echoing).
 *
 * REST (HttpServer) stays the source of truth for history/CRUD; this
 * class only pushes what's posted while a client is connected.
 */
class WebSocketServer {
public:
    WebSocketServer(ChatService& chatService, const AuthServiceClient& authServiceClient, int port,
                     const std::string& host = "127.0.0.1");

    /// Starts accepting connections; returns once listening (async from there).
    bool start();

    /// Stops accepting connections and closes existing ones.
    void stop();

private:
    struct Subscription {
        std::string login;
        std::int64_t channelId = 0;
    };

    void handleMessage(const std::shared_ptr<ix::ConnectionState>& connectionState, ix::WebSocket& webSocket,
                        const ix::WebSocketMessagePtr& message);
    void handleHello(ix::WebSocket& webSocket, const std::string& payload);
    void handleChatMessage(ix::WebSocket& webSocket, const std::string& payload);
    void broadcastToChannel(std::int64_t channelId, const std::string& json);

    ChatService& chatService_;
    const AuthServiceClient& authServiceClient_;
    ix::WebSocketServer server_;

    std::mutex subscriptionsMutex_;
    std::unordered_map<ix::WebSocket*, Subscription> subscriptions_;
};

}  // namespace chat_service
