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
 * @brief Real-time delivery of new messages over WebSocket, plus call
 *        signaling relay for group voice calls (issue #46).
 *
 * Protocol: the first message a client sends must be
 * `{"token": "...", "channel_id": N}` — verified against auth-service
 * (AuthServiceClient) before the connection is subscribed to that
 * channel. Every message after that is one of:
 *   - `{"body": "..."}` — chat message: persisted via ChatService, then
 *     broadcast as JSON to every connection subscribed to the same
 *     channel (including the sender, so all clients render off the
 *     same real-time stream rather than optimistically local-echoing).
 *   - `{"call_join": true}` — join the voice call for the subscribed
 *     channel; replies with `{"call_roster": [...]}` (existing call
 *     participants, not persisted, ephemeral to this process) and
 *     broadcasts `{"call_peer_joined": "<login>"}` to them.
 *   - `{"call_leave": true}` — leave the call; broadcasts
 *     `{"call_peer_left": "<login>"}` to the remaining participants.
 *     Disconnecting (Close/Error) without an explicit leave has the
 *     same effect.
 *   - `{"call_signal": {"to": "<login>", "payload": {...}}}` — opaque
 *     signaling data (SDP offer/answer, ICE candidate) relayed
 *     verbatim to the named participant as
 *     `{"call_signal": {"from": "<login>", "payload": {...}}}` — this
 *     class never inspects `payload`. Replies `{"error": "peer not in
 *     call"}` to the sender if `to` isn't a current call participant.
 *   - `{"typing": true}` — issue #96: broadcasts
 *     `{"user_typing": "<login>"}` to every other subscriber of the
 *     same channel (never back to the sender). Ephemeral, like call
 *     presence — nothing persisted, no explicit "stopped typing"
 *     message; the client side times the indicator out on its own.
 *
 * REST (HttpServer) stays the source of truth for history/CRUD; this
 * class only pushes what's posted while a client is connected, and
 * only relays call signaling — it never decodes SDP/ICE content.
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
    void handleSubscribedMessage(ix::WebSocket& webSocket, const std::string& payload);
    void handleChatMessage(ix::WebSocket& webSocket, const Subscription& subscription, const nlohmann::json& body);
    void handleCallJoin(ix::WebSocket& webSocket, const Subscription& subscription);
    void handleCallLeave(ix::WebSocket& webSocket, const Subscription& subscription);
    void handleCallSignal(ix::WebSocket& webSocket, const Subscription& subscription, const nlohmann::json& body);
    void handleTyping(ix::WebSocket& webSocket, const Subscription& subscription);
    void removeCallParticipant(const Subscription& subscription, ix::WebSocket* socket);
    /// Sends @p json to every socket subscribed to @p channelId's chat,
    /// except @p excludeSocket (nullptr — the default — excludes none,
    /// which is what a normal chat message broadcast wants; typing
    /// notifications pass the sender here so they don't see their own
    /// "typing" echoed back).
    void broadcastToChannel(std::int64_t channelId, const std::string& json, const ix::WebSocket* excludeSocket = nullptr);
    /// Unlike broadcastToChannel (all channel *chat* subscribers), this
    /// only reaches sockets actually in callParticipants_[channelId] —
    /// someone subscribed to the channel's text chat but not in the
    /// call shouldn't see call presence traffic. Never sends to @p
    /// excludeSocket (typically the participant who just triggered the
    /// notification).
    void broadcastToCallParticipants(std::int64_t channelId, const std::string& json,
                                      const ix::WebSocket* excludeSocket);

    ChatService& chatService_;
    const AuthServiceClient& authServiceClient_;
    ix::WebSocketServer server_;

    std::mutex subscriptionsMutex_;
    std::unordered_map<ix::WebSocket*, Subscription> subscriptions_;
    // channelId -> login -> socket. Ephemeral call presence, separate
    // from channel *chat* subscription above — a client can be
    // subscribed to a channel's text chat without being in its call.
    std::unordered_map<std::int64_t, std::unordered_map<std::string, ix::WebSocket*>> callParticipants_;
};

}  // namespace chat_service
