#pragma once

#include "ChatRepository.h"

namespace chat_service {

/**
 * @brief Бизнес-логика для сообществ/каналов/сообщений.
 *
 * Делегирует хранение ChatRepository; вызывающая сторона уже должна быть
 * аутентифицирована (см. AuthServiceClient) — этот класс знает только
 * строку "login", а не токены.
 */
class ChatService {
public:
    explicit ChatService(ChatRepository& repository);

    [[nodiscard]] Community createCommunity(const std::string& name, const std::string& ownerLogin);
    [[nodiscard]] std::vector<Community> listCommunities();
    [[nodiscard]] std::vector<Community> listCommunitiesForMember(const std::string& login);
    [[nodiscard]] std::optional<Community> findCommunityByInviteCode(const std::string& code);
    [[nodiscard]] RegenerateInviteCodeResult regenerateInviteCode(std::int64_t communityId,
                                                                    const std::string& requesterLogin);
    [[nodiscard]] MutationResult renameCommunity(std::int64_t id, const std::string& newName,
                                                  const std::string& requesterLogin);
    [[nodiscard]] MutationResult deleteCommunity(std::int64_t id, const std::string& requesterLogin);

    [[nodiscard]] std::optional<std::int64_t> createChannel(std::int64_t communityId, const std::string& name,
                                                             const std::string& ownerLogin,
                                                             bool isEncrypted = false);
    [[nodiscard]] std::vector<Channel> listChannels(std::int64_t communityId);
    [[nodiscard]] std::optional<Channel> findChannel(std::int64_t id);
    [[nodiscard]] std::vector<std::string> listMembers(std::int64_t communityId);
    [[nodiscard]] bool isMember(std::int64_t communityId, const std::string& login);
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
                                                       std::optional<std::int64_t> attachmentId = std::nullopt,
                                                       std::optional<std::int64_t> replyToMessageId = std::nullopt);
    [[nodiscard]] std::vector<Message> recentMessages(std::int64_t channelId, int limit,
                                                        std::optional<std::int64_t> beforeId = std::nullopt);
    [[nodiscard]] EditMessageResult editMessage(std::int64_t messageId, std::int64_t channelId,
                                                 const std::string& requesterLogin, const std::string& newBody);
    [[nodiscard]] MutationResult deleteMessage(std::int64_t messageId, std::int64_t channelId,
                                                const std::string& requesterLogin);

    /// См. ChatRepository::pinMessage()/unpinMessage()/listPinnedMessages() (issue #338).
    [[nodiscard]] PinMessageResult pinMessage(std::int64_t messageId, std::int64_t channelId,
                                               const std::string& requesterLogin);
    [[nodiscard]] MutationResult unpinMessage(std::int64_t messageId, std::int64_t channelId,
                                               const std::string& requesterLogin);
    [[nodiscard]] std::vector<PinnedMessage> listPinnedMessages(std::int64_t channelId);

    [[nodiscard]] std::vector<Message> searchMessages(std::int64_t channelId, const std::string& query, int limit);

    /// См. ChatRepository::toggleReaction() (issue #333).
    [[nodiscard]] ToggleReactionResult toggleReaction(std::int64_t messageId, std::int64_t channelId,
                                                       const std::string& login, const std::string& emoji);

    [[nodiscard]] std::optional<AttachmentMetadata> createAttachment(std::int64_t channelId,
                                                                       const AttachmentUpload& upload);
    [[nodiscard]] std::optional<AttachmentData> findAttachmentData(std::int64_t attachmentId);

    /// См. ChatRepository::findOrCreateThread() (issue #187, Фаза 2) —
    /// дружба проверяется в HttpServer до вызова этого метода.
    [[nodiscard]] std::int64_t findOrCreateThread(const std::string& loginA, const std::string& loginB);
    [[nodiscard]] std::vector<DirectMessageThread> listMyThreads(const std::string& login);
    [[nodiscard]] bool isThreadParticipant(std::int64_t threadId, const std::string& login);
    [[nodiscard]] std::optional<DirectMessage> postDirectMessage(std::int64_t threadId,
                                                                  const std::string& authorLogin,
                                                                  const std::string& body);
    [[nodiscard]] std::vector<DirectMessage> listDirectMessages(std::int64_t threadId, int limit,
                                                                 std::optional<std::int64_t> beforeId = std::nullopt);

    /// @return True, если @p login состоит в сообществе канала @p channelId
    /// — переиспользуется существующая проверка членства (issue #231),
    /// а не заводится отдельная система прав для звонков; см.
    /// ChatRepository::isChannelMember().
    [[nodiscard]] bool isChannelMember(std::int64_t channelId, const std::string& login);

    /// Идемпотентно фиксирует, что @p channelId использует videoroom
    /// @p janusRoomId — вызывается после того, как WebSocketServer
    /// (владеющий JanusClient) убедился, что комната реально существует в
    /// Janus; см. doc-комментарий ChatRepository::recordJanusRoom().
    void recordCallRoom(std::int64_t channelId, const std::string& janusRoomId);

    /// Issue #310/#348 — см. doc-комментарии соответствующих методов
    /// ChatRepository.
    void markChannelRead(std::int64_t channelId, const std::string& login, std::int64_t messageId);
    void markDmThreadRead(std::int64_t threadId, const std::string& login, std::int64_t messageId);
    [[nodiscard]] std::vector<ChannelUnreadCount> listUnreadChannelCounts(const std::string& login);
    [[nodiscard]] std::vector<ThreadUnreadCount> listUnreadThreadCounts(const std::string& login);

private:
    ChatRepository& repository_;
};

}  // namespace chat_service
