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

    [[nodiscard]] Community createCommunity(const std::string& name);
    [[nodiscard]] std::vector<Community> listCommunities();

    [[nodiscard]] std::optional<std::int64_t> createChannel(std::int64_t communityId, const std::string& name);
    [[nodiscard]] std::vector<Channel> listChannels(std::int64_t communityId);

    [[nodiscard]] bool joinCommunity(std::int64_t communityId, const std::string& login);

    [[nodiscard]] std::optional<Message> postMessage(std::int64_t channelId, const std::string& authorLogin,
                                                       const std::string& body);
    [[nodiscard]] std::vector<Message> recentMessages(std::int64_t channelId, int limit);

private:
    ChatRepository& repository_;
};

}  // namespace chat_service
