#pragma once

#include <httplib.h>

#include <optional>
#include <string>

#include "AuthServiceClient.h"
#include "ChatService.h"

namespace chat_service {

/**
 * @brief REST front-end for ChatService: communities, channels, joining,
 *        moderator management (issue #114), message history,
 *        attachment upload/download (issue #116), message search
 *        (issue #118), and per-member channel-key exchange for E2E
 *        encryption (issue #138). Every route requires a valid
 *        `Authorization: Bearer <token>` header, checked against
 *        auth-service via AuthServiceClient.
 *
 * Promoting/demoting a moderator is owner-only — writeMutationResult()'s
 * kForbidden ("only the owner can do that") already covers it, same as
 * rename/delete community.
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
    void handlePromoteModerator(const httplib::Request& request, httplib::Response& response);
    void handleDemoteModerator(const httplib::Request& request, httplib::Response& response);
    void handleListModerators(const httplib::Request& request, httplib::Response& response);
    void handleCreateChannel(const httplib::Request& request, httplib::Response& response);
    void handleListChannels(const httplib::Request& request, httplib::Response& response);
    void handleRenameChannel(const httplib::Request& request, httplib::Response& response);
    void handleDeleteChannel(const httplib::Request& request, httplib::Response& response);
    void handleListMessages(const httplib::Request& request, httplib::Response& response);
    void handleUploadAttachment(const httplib::Request& request, httplib::Response& response);
    void handleDownloadAttachment(const httplib::Request& request, httplib::Response& response);
    void handleSearchMessages(const httplib::Request& request, httplib::Response& response);
    void handleListMembers(const httplib::Request& request, httplib::Response& response);
    void handleSetChannelKey(const httplib::Request& request, httplib::Response& response);
    void handleGetMyChannelKey(const httplib::Request& request, httplib::Response& response);
    void writeMutationResult(MutationResult result, httplib::Response& response);

    ChatService& chatService_;
    const AuthServiceClient& authServiceClient_;
    httplib::Server server_;
};

}  // namespace chat_service
