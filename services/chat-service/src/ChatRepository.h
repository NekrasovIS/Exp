#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace chat_service {

struct Community {
    std::int64_t id = 0;
    std::string name;
    std::string ownerLogin;
};

struct Channel {
    std::int64_t id = 0;
    std::int64_t communityId = 0;
    std::string name;
    std::string ownerLogin;
};

/// Outcome of a rename/delete attempt — distinguishes "doesn't exist"
/// from "exists but you don't own it" so HTTP handlers can answer with
/// the right status code (404 vs 403) instead of collapsing both to a
/// single generic failure.
enum class MutationResult {
    kSuccess,
    kNotFound,
    kForbidden,
    kConflict,  // e.g. renaming a channel to a name already taken in the same community
};

struct Message {
    std::int64_t id = 0;
    std::string authorLogin;
    std::string body;
    std::string sentAt;                   // ISO 8601, as returned by Postgres
    std::optional<std::string> editedAt;  // unset if never edited (issue #107)
};

/// Outcome of editMessage() — MutationResult alone doesn't carry the
/// new edited_at Postgres assigned, which WebSocketServer needs for
/// the broadcast payload.
struct EditMessageResult {
    MutationResult result = MutationResult::kNotFound;
    std::string editedAt;  // only meaningful when result == kSuccess
};

/**
 * @brief Postgres-backed storage for communities/channels/memberships/messages.
 *
 * Opens a fresh connection per call rather than pooling — pqxx::connection
 * isn't thread-safe and HttpServer/WebSocketServer may dispatch from
 * multiple threads; see UserRepository in user-service for the same
 * tradeoff and its rationale.
 *
 * Roles (issue #114): every community has exactly one owner
 * (communities.owner_login, set at creation, never transferable) plus
 * any number of moderators (memberships.is_moderator) the owner
 * promotes/demotes. A moderator can delete any message and
 * rename/delete any channel within that one community — never touch
 * the community itself (rename/delete community, promote/demote other
 * moderators) and never edit someone else's message (see
 * editMessage()'s doc comment). Moderator status doesn't cross
 * communities: being a moderator of one grants nothing in another.
 */
class ChatRepository {
public:
    explicit ChatRepository(std::string connectionString);

    [[nodiscard]] Community createCommunity(const std::string& name, const std::string& ownerLogin);
    [[nodiscard]] std::vector<Community> listCommunities();
    [[nodiscard]] MutationResult renameCommunity(std::int64_t id, const std::string& newName,
                                                  const std::string& requesterLogin);
    [[nodiscard]] MutationResult deleteCommunity(std::int64_t id, const std::string& requesterLogin);

    /// @return The new channel's id, or std::nullopt if @p communityId doesn't exist
    ///         or already has a channel named @p name.
    [[nodiscard]] std::optional<std::int64_t> createChannel(std::int64_t communityId, const std::string& name,
                                                             const std::string& ownerLogin);
    [[nodiscard]] std::vector<Channel> listChannels(std::int64_t communityId);

    /// Allowed for the channel's own owner, the parent community's
    /// owner, or a moderator of the parent community (issue #114) — not
    /// just whoever happens to have created this particular channel.
    [[nodiscard]] MutationResult renameChannel(std::int64_t id, const std::string& newName,
                                                const std::string& requesterLogin);

    /// Same authority rule as renameChannel().
    [[nodiscard]] MutationResult deleteChannel(std::int64_t id, const std::string& requesterLogin);

    /// Idempotent: joining a community you're already in succeeds.
    /// @return False if @p communityId doesn't exist.
    [[nodiscard]] bool joinCommunity(std::int64_t communityId, const std::string& login);

    /// Promotes @p targetLogin to moderator of @p communityId — owner-only
    /// (issue #114). Implicitly joins @p targetLogin to the community
    /// first if they weren't already a member (same effect as
    /// joinCommunity()) — a moderator who isn't even a member would be a
    /// confusing state to end up in.
    [[nodiscard]] MutationResult promoteModerator(std::int64_t communityId, const std::string& targetLogin,
                                                   const std::string& requesterLogin);

    /// Owner-only. Demoting someone who was never a moderator (or isn't
    /// even a member) is a harmless no-op success, not an error — same
    /// idempotent style as joinCommunity().
    [[nodiscard]] MutationResult demoteModerator(std::int64_t communityId, const std::string& targetLogin,
                                                  const std::string& requesterLogin);

    /// Logins of every current moderator of @p communityId, in no
    /// particular order — empty (not an error) for a nonexistent
    /// community, same "just return nothing" style as
    /// listChannels()/listCommunities().
    [[nodiscard]] std::vector<std::string> listModerators(std::int64_t communityId);

    /// @return The stored message (with its assigned id/timestamp), or
    ///         std::nullopt if @p channelId doesn't exist.
    [[nodiscard]] std::optional<Message> insertMessage(std::int64_t channelId, const std::string& authorLogin,
                                                        const std::string& body);
    /// Chronological (oldest to newest) page of up to @p limit messages.
    /// With @p beforeId unset, the page ends at the newest message in
    /// the channel; with it set, the page ends just before that
    /// message's id — lets a caller page backwards through history by
    /// passing the id of the oldest message it already has.
    [[nodiscard]] std::vector<Message> listRecentMessages(std::int64_t channelId, int limit,
                                                            std::optional<std::int64_t> beforeId = std::nullopt);

    /// Only @p requesterLogin being the message's own author may edit
    /// it (kForbidden otherwise) — not the channel/community owner, and
    /// not a moderator either, even after issue #114: editing someone
    /// else's message would let a moderator put words in their mouth,
    /// which is a materially different (and worse) power than removing
    /// content outright. See WebSocketServer's class doc comment for
    /// the moderation-vs-authorship distinction this deliberately
    /// doesn't blur.
    [[nodiscard]] EditMessageResult editMessage(std::int64_t messageId, std::int64_t channelId,
                                                 const std::string& requesterLogin, const std::string& newBody);

    /// Unlike editMessage(), deletion IS available to the parent
    /// channel/community's owner or a community moderator (issue #114)
    /// — removing bad content is ordinary moderation; rewriting it
    /// isn't.
    [[nodiscard]] MutationResult deleteMessage(std::int64_t messageId, std::int64_t channelId,
                                                const std::string& requesterLogin);

private:
    std::string connectionString_;
};

}  // namespace chat_service
