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
                                                             const std::string& ownerLogin,
                                                             bool isEncrypted = false);
    [[nodiscard]] std::vector<Channel> listChannels(std::int64_t communityId);
    [[nodiscard]] std::optional<Channel> findChannel(std::int64_t id);
    [[nodiscard]] std::vector<std::string> listMembers(std::int64_t communityId);
    [[nodiscard]] MutationResult renameChannel(std::int64_t id, const std::string& newName,
                                                const std::string& requesterLogin);
    [[nodiscard]] MutationResult deleteChannel(std::int64_t id, const std::string& requesterLogin);

    [[nodiscard]] MutationResult setChannelKey(std::int64_t channelId, const std::string& memberLogin,
                                                const std::string& requesterLogin, const std::string& wrappedKey);
    [[nodiscard]] std::optional<std::string> findChannelKey(std::int64_t channelId, const std::string& login);

    [[nodiscard]] bool joinCommunity(std::int64_t communityId, const std::string& login);

    [[nodiscard]] MutationResult promoteModerator(std::int64_t communityId, const std::string& targetLogin,
                                                   const std::string& requesterLogin);
    [[nodiscard]] MutationResult demoteModerator(std::int64_t communityId, const std::string& targetLogin,
                                                  const std::string& requesterLogin);
    [[nodiscard]] std::vector<std::string> listModerators(std::int64_t communityId);

    [[nodiscard]] std::optional<Message> postMessage(std::int64_t channelId, const std::string& authorLogin,
                                                       const std::string& body,
                                                       std::optional<std::int64_t> attachmentId = std::nullopt);
    [[nodiscard]] std::vector<Message> recentMessages(std::int64_t channelId, int limit,
                                                        std::optional<std::int64_t> beforeId = std::nullopt);
    [[nodiscard]] EditMessageResult editMessage(std::int64_t messageId, std::int64_t channelId,
                                                 const std::string& requesterLogin, const std::string& newBody);
    [[nodiscard]] MutationResult deleteMessage(std::int64_t messageId, std::int64_t channelId,
                                                const std::string& requesterLogin);
    [[nodiscard]] std::vector<Message> searchMessages(std::int64_t channelId, const std::string& query, int limit);

    [[nodiscard]] std::optional<AttachmentMetadata> createAttachment(std::int64_t channelId,
                                                                       const std::string& uploaderLogin,
                                                                       const std::string& filename,
                                                                       const std::string& contentType,
                                                                       const std::string& dataBase64,
                                                                       std::int64_t sizeBytes);
    [[nodiscard]] std::optional<AttachmentData> findAttachmentData(std::int64_t attachmentId);

private:
    ChatRepository& repository_;
};

}  // namespace chat_service
