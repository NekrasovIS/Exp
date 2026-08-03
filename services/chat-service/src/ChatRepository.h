#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace chat_service {

struct Community {
    std::int64_t id = 0;
    std::string name;
};

struct Channel {
    std::int64_t id = 0;
    std::int64_t communityId = 0;
    std::string name;
};

struct Message {
    std::int64_t id = 0;
    std::string authorLogin;
    std::string body;
    std::string sentAt;  // ISO 8601, as returned by Postgres
};

/**
 * @brief Postgres-backed storage for communities/channels/memberships/messages.
 *
 * Opens a fresh connection per call rather than pooling — pqxx::connection
 * isn't thread-safe and HttpServer/WebSocketServer may dispatch from
 * multiple threads; see UserRepository in user-service for the same
 * tradeoff and its rationale.
 */
class ChatRepository {
public:
    explicit ChatRepository(std::string connectionString);

    [[nodiscard]] Community createCommunity(const std::string& name);
    [[nodiscard]] std::vector<Community> listCommunities();

    /// @return The new channel's id, or std::nullopt if @p communityId doesn't exist
    ///         or already has a channel named @p name.
    [[nodiscard]] std::optional<std::int64_t> createChannel(std::int64_t communityId, const std::string& name);
    [[nodiscard]] std::vector<Channel> listChannels(std::int64_t communityId);

    /// Idempotent: joining a community you're already in succeeds.
    /// @return False if @p communityId doesn't exist.
    [[nodiscard]] bool joinCommunity(std::int64_t communityId, const std::string& login);

    /// @return The stored message (with its assigned id/timestamp), or
    ///         std::nullopt if @p channelId doesn't exist.
    [[nodiscard]] std::optional<Message> insertMessage(std::int64_t channelId, const std::string& authorLogin,
                                                        const std::string& body);
    [[nodiscard]] std::vector<Message> listRecentMessages(std::int64_t channelId, int limit);

private:
    std::string connectionString_;
};

}  // namespace chat_service
