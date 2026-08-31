#include "ChatService.h"

namespace chat_service {

ChatService::ChatService(ChatRepository& repository) : repository_(repository) {}

Community ChatService::createCommunity(const std::string& name, const std::string& ownerLogin) {
    return repository_.createCommunity(name, ownerLogin);
}

std::vector<Community> ChatService::listCommunities() {
    return repository_.listCommunities();
}

MutationResult ChatService::renameCommunity(std::int64_t id, const std::string& newName,
                                             const std::string& requesterLogin) {
    return repository_.renameCommunity(id, newName, requesterLogin);
}

MutationResult ChatService::deleteCommunity(std::int64_t id, const std::string& requesterLogin) {
    return repository_.deleteCommunity(id, requesterLogin);
}

std::optional<std::int64_t> ChatService::createChannel(std::int64_t communityId, const std::string& name,
                                                         const std::string& ownerLogin) {
    return repository_.createChannel(communityId, name, ownerLogin);
}

std::vector<Channel> ChatService::listChannels(std::int64_t communityId) {
    return repository_.listChannels(communityId);
}

MutationResult ChatService::renameChannel(std::int64_t id, const std::string& newName,
                                           const std::string& requesterLogin) {
    return repository_.renameChannel(id, newName, requesterLogin);
}

MutationResult ChatService::deleteChannel(std::int64_t id, const std::string& requesterLogin) {
    return repository_.deleteChannel(id, requesterLogin);
}

bool ChatService::joinCommunity(std::int64_t communityId, const std::string& login) {
    return repository_.joinCommunity(communityId, login);
}

std::optional<Message> ChatService::postMessage(std::int64_t channelId, const std::string& authorLogin,
                                                  const std::string& body, std::optional<std::int64_t> attachmentId) {
    return repository_.insertMessage(channelId, authorLogin, body, attachmentId);
}

std::vector<Message> ChatService::recentMessages(std::int64_t channelId, int limit, std::optional<std::int64_t> beforeId) {
    return repository_.listRecentMessages(channelId, limit, beforeId);
}

EditMessageResult ChatService::editMessage(std::int64_t messageId, std::int64_t channelId,
                                            const std::string& requesterLogin, const std::string& newBody) {
    return repository_.editMessage(messageId, channelId, requesterLogin, newBody);
}

MutationResult ChatService::deleteMessage(std::int64_t messageId, std::int64_t channelId,
                                           const std::string& requesterLogin) {
    return repository_.deleteMessage(messageId, channelId, requesterLogin);
}

std::optional<AttachmentMetadata> ChatService::createAttachment(std::int64_t channelId,
                                                                   const std::string& uploaderLogin,
                                                                   const std::string& filename,
                                                                   const std::string& contentType,
                                                                   const std::string& dataBase64,
                                                                   std::int64_t sizeBytes) {
    return repository_.createAttachment(channelId, uploaderLogin, filename, contentType, dataBase64, sizeBytes);
}

std::optional<AttachmentData> ChatService::findAttachmentData(std::int64_t attachmentId) {
    return repository_.findAttachmentData(attachmentId);
}

}  // namespace chat_service
