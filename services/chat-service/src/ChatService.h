#pragma once

#include "ChatRepository.h"

namespace chat_service {

/**
 * @brief Business logic for communities/channels/messages.
 *
 * Delegates storage to ChatRepository; callers must already be
 * authenticated (see AuthServiceClient) — this class only knows about
 * a "login" string, not tokens.
 */
class ChatService {
public:
    explicit ChatService(ChatRepository& repository);

    [[nodiscard]] Community createCommunity(const std::string& name, const std::string& ownerLogin);
    [[nodiscard]] std::vector<Community> listCommunities();
    [[nodiscard]] MutationResult renameCommunity(std::int64_t id, const std::string& newName,
                                                  const std::string& requesterLogin);
    [[nodiscard]] MutationResult deleteCommunity(std::int64_t id, const std::string& requesterLogin);

    [[nodiscard]] std::optional<std::int64_t> createChannel(std::int64_t communityId, const std::string& name,
                                                             const std::string& ownerLogin);
    [[nodiscard]] std::vector<Channel> listChannels(std::int64_t communityId);
    [[nodiscard]] MutationResult renameChannel(std::int64_t id, const std::string& newName,
                                                const std::string& requesterLogin);
    [[nodiscard]] MutationResult deleteChannel(std::int64_t id, const std::string& requesterLogin);

    [[nodiscard]] bool joinCommunity(std::int64_t communityId, const std::string& login);

    [[nodiscard]] std::optional<Message> postMessage(std::int64_t channelId, const std::string& authorLogin,
                                                       const std::string& body);
    [[nodiscard]] std::vector<Message> recentMessages(std::int64_t channelId, int limit);
    [[nodiscard]] EditMessageResult editMessage(std::int64_t messageId, std::int64_t channelId,
                                                 const std::string& requesterLogin, const std::string& newBody);
    [[nodiscard]] MutationResult deleteMessage(std::int64_t messageId, std::int64_t channelId,
                                                const std::string& requesterLogin);

private:
    ChatRepository& repository_;
};

}  // namespace chat_service
