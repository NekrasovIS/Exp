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
                                                         const std::string& ownerLogin, bool isEncrypted) {
    return repository_.createChannel(communityId, name, ownerLogin, isEncrypted);
}

std::vector<Channel> ChatService::listChannels(std::int64_t communityId) {
    return repository_.listChannels(communityId);
}

std::optional<Channel> ChatService::findChannel(std::int64_t id) {
    return repository_.findChannel(id);
}

std::vector<std::string> ChatService::listMembers(std::int64_t communityId) {
    return repository_.listMembers(communityId);
}

MutationResult ChatService::renameChannel(std::int64_t id, const std::string& newName,
                                           const std::string& requesterLogin) {
    return repository_.renameChannel(id, newName, requesterLogin);
}

MutationResult ChatService::deleteChannel(std::int64_t id, const std::string& requesterLogin) {
    return repository_.deleteChannel(id, requesterLogin);
}

MutationResult ChatService::setChannelKey(std::int64_t channelId, const std::string& memberLogin,
                                           const std::string& requesterLogin, const std::string& wrappedKey) {
    return repository_.setChannelKey(channelId, memberLogin, requesterLogin, wrappedKey);
}

std::optional<std::string> ChatService::findChannelKey(std::int64_t channelId, const std::string& login) {
    return repository_.findChannelKey(channelId, login);
}

bool ChatService::joinCommunity(std::int64_t communityId, const std::string& login) {
    return repository_.joinCommunity(communityId, login);
}

MutationResult ChatService::promoteModerator(std::int64_t communityId, const std::string& targetLogin,
                                              const std::string& requesterLogin) {
    return repository_.promoteModerator(communityId, targetLogin, requesterLogin);
}

MutationResult ChatService::demoteModerator(std::int64_t communityId, const std::string& targetLogin,
                                             const std::string& requesterLogin) {
    return repository_.demoteModerator(communityId, targetLogin, requesterLogin);
}

std::vector<std::string> ChatService::listModerators(std::int64_t communityId) {
    return repository_.listModerators(communityId);
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
                                                                   const AttachmentUpload& upload) {
    return repository_.createAttachment(channelId, upload);
}

std::optional<AttachmentData> ChatService::findAttachmentData(std::int64_t attachmentId) {
    return repository_.findAttachmentData(attachmentId);
}

std::vector<Message> ChatService::searchMessages(std::int64_t channelId, const std::string& query, int limit) {
    return repository_.searchMessages(channelId, query, limit);
}

}  // namespace chat_service
