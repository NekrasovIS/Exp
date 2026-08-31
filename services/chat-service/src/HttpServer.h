#pragma once

#include <httplib.h>

#include <optional>
#include <string>

#include "AuthServiceClient.h"
#include "ChatService.h"

namespace chat_service {

/**
 * @brief REST front-end for ChatService: communities, channels, joining,
 *        message history, and attachment upload/download (issue #116).
 *        Every route requires a valid `Authorization: Bearer <token>`
 *        header, checked against auth-service via AuthServiceClient.
 *
 * Real-time message delivery is WebSocketServer's job, not this class's
 * — this is history/CRUD only.
 */
class HttpServer {
public:
    HttpServer(ChatService& chatService, const AuthServiceClient& authServiceClient);

    /// Blocks, serving requests until stop() is called from another thread.
    void listen(const std::string& host, int port);

    /// Stops a listen() call in progress.
    void stop();

private:
    void registerRoutes();
    [[nodiscard]] std::optional<std::string> authenticate(const httplib::Request& request) const;

    void handleCreateCommunity(const httplib::Request& request, httplib::Response& response);
    void handleListCommunities(const httplib::Request& request, httplib::Response& response);
    void handleRenameCommunity(const httplib::Request& request, httplib::Response& response);
    void handleDeleteCommunity(const httplib::Request& request, httplib::Response& response);
    void handleJoinCommunity(const httplib::Request& request, httplib::Response& response);
    void handleCreateChannel(const httplib::Request& request, httplib::Response& response);
    void handleListChannels(const httplib::Request& request, httplib::Response& response);
    void handleRenameChannel(const httplib::Request& request, httplib::Response& response);
    void handleDeleteChannel(const httplib::Request& request, httplib::Response& response);
    void handleListMessages(const httplib::Request& request, httplib::Response& response);
    void handleUploadAttachment(const httplib::Request& request, httplib::Response& response);
    void handleDownloadAttachment(const httplib::Request& request, httplib::Response& response);
    void writeMutationResult(MutationResult result, httplib::Response& response);

    ChatService& chatService_;
    const AuthServiceClient& authServiceClient_;
    httplib::Server server_;
};

}  // namespace chat_service
