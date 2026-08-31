#include "ChatRepository.h"

#include <pqxx/pqxx>

#include <algorithm>

namespace chat_service {

namespace {

/// @return True if @p login is a moderator of @p communityId, within an
/// already-open @p transaction — shared by deleteMessage()/
/// renameChannel()/deleteChannel(), each of which needs this same
/// check alongside their own owner check.
bool isModerator(pqxx::work& transaction, std::int64_t communityId, const std::string& login) {
    const pqxx::result rows =
        transaction.exec("SELECT is_moderator FROM memberships WHERE community_id = $1 AND member_login = $2",
                          pqxx::params{communityId, login});
    return !rows.empty() && rows[0][0].as<bool>();
}

}  // namespace

ChatRepository::ChatRepository(std::string connectionString) : connectionString_(std::move(connectionString)) {}

Community ChatRepository::createCommunity(const std::string& name, const std::string& ownerLogin) {
    pqxx::connection connection(connectionString_);
    pqxx::work transaction(connection);

    const pqxx::result rows = transaction.exec(
        "INSERT INTO communities (name, owner_login) VALUES ($1, $2) RETURNING id", pqxx::params{name, ownerLogin});
    transaction.commit();

    return Community{.id = rows[0][0].as<std::int64_t>(), .name = name, .ownerLogin = ownerLogin};
}

std::vector<Community> ChatRepository::listCommunities() {
    pqxx::connection connection(connectionString_);
    pqxx::work transaction(connection);

    const pqxx::result rows = transaction.exec("SELECT id, name, owner_login FROM communities ORDER BY id");

    std::vector<Community> communities;
    communities.reserve(static_cast<std::size_t>(rows.size()));
    for (const auto& row : rows) {
        communities.push_back(Community{.id = row[0].as<std::int64_t>(),
                                         .name = row[1].as<std::string>(),
                                         .ownerLogin = row[2].as<std::string>()});
    }
    return communities;
}

MutationResult ChatRepository::renameCommunity(std::int64_t id, const std::string& newName,
                                                const std::string& requesterLogin) {
    pqxx::connection connection(connectionString_);
    pqxx::work transaction(connection);

    const pqxx::result ownerRows =
        transaction.exec("SELECT owner_login FROM communities WHERE id = $1", pqxx::params{id});
    if (ownerRows.empty()) {
        return MutationResult::kNotFound;
    }
    if (ownerRows[0][0].as<std::string>() != requesterLogin) {
        return MutationResult::kForbidden;
    }

    transaction.exec("UPDATE communities SET name = $1 WHERE id = $2", pqxx::params{newName, id});
    transaction.commit();
    return MutationResult::kSuccess;
}

MutationResult ChatRepository::deleteCommunity(std::int64_t id, const std::string& requesterLogin) {
    pqxx::connection connection(connectionString_);
    pqxx::work transaction(connection);

    const pqxx::result ownerRows =
        transaction.exec("SELECT owner_login FROM communities WHERE id = $1", pqxx::params{id});
    if (ownerRows.empty()) {
        return MutationResult::kNotFound;
    }
    if (ownerRows[0][0].as<std::string>() != requesterLogin) {
        return MutationResult::kForbidden;
    }

    // channels/memberships/messages cascade via ON DELETE CASCADE (see db/init.sql).
    transaction.exec("DELETE FROM communities WHERE id = $1", pqxx::params{id});
    transaction.commit();
    return MutationResult::kSuccess;
}

std::optional<std::int64_t> ChatRepository::createChannel(std::int64_t communityId, const std::string& name,
                                                            const std::string& ownerLogin) {
    pqxx::connection connection(connectionString_);
    pqxx::work transaction(connection);

    try {
        const pqxx::result rows =
            transaction.exec("INSERT INTO channels (community_id, name, owner_login) VALUES ($1, $2, $3) "
                              "RETURNING id",
                              pqxx::params{communityId, name, ownerLogin});
        transaction.commit();
        return rows[0][0].as<std::int64_t>();
    } catch (const pqxx::foreign_key_violation&) {
        return std::nullopt;
    } catch (const pqxx::unique_violation&) {
        return std::nullopt;
    }
}

std::vector<Channel> ChatRepository::listChannels(std::int64_t communityId) {
    pqxx::connection connection(connectionString_);
    pqxx::work transaction(connection);

    const pqxx::result rows =
        transaction.exec("SELECT id, community_id, name, owner_login FROM channels WHERE community_id = $1 "
                          "ORDER BY id",
                          pqxx::params{communityId});

    std::vector<Channel> channels;
    channels.reserve(static_cast<std::size_t>(rows.size()));
    for (const auto& row : rows) {
        channels.push_back(Channel{.id = row[0].as<std::int64_t>(),
                                    .communityId = row[1].as<std::int64_t>(),
                                    .name = row[2].as<std::string>(),
                                    .ownerLogin = row[3].as<std::string>()});
    }
    return channels;
}

MutationResult ChatRepository::renameChannel(std::int64_t id, const std::string& newName,
                                              const std::string& requesterLogin) {
    pqxx::connection connection(connectionString_);
    pqxx::work transaction(connection);

    const pqxx::result rows = transaction.exec(
        "SELECT ch.owner_login, ch.community_id, co.owner_login FROM channels ch "
        "JOIN communities co ON co.id = ch.community_id WHERE ch.id = $1",
        pqxx::params{id});
    if (rows.empty()) {
        return MutationResult::kNotFound;
    }
    const std::string channelOwner = rows[0][0].as<std::string>();
    const auto communityId = rows[0][1].as<std::int64_t>();
    const std::string communityOwner = rows[0][2].as<std::string>();
    if (requesterLogin != channelOwner && requesterLogin != communityOwner &&
        !isModerator(transaction, communityId, requesterLogin)) {
        return MutationResult::kForbidden;
    }

    try {
        transaction.exec("UPDATE channels SET name = $1 WHERE id = $2", pqxx::params{newName, id});
        transaction.commit();
        return MutationResult::kSuccess;
    } catch (const pqxx::unique_violation&) {
        return MutationResult::kConflict;
    }
}

MutationResult ChatRepository::deleteChannel(std::int64_t id, const std::string& requesterLogin) {
    pqxx::connection connection(connectionString_);
    pqxx::work transaction(connection);

    const pqxx::result rows = transaction.exec(
        "SELECT ch.owner_login, ch.community_id, co.owner_login FROM channels ch "
        "JOIN communities co ON co.id = ch.community_id WHERE ch.id = $1",
        pqxx::params{id});
    if (rows.empty()) {
        return MutationResult::kNotFound;
    }
    const std::string channelOwner = rows[0][0].as<std::string>();
    const auto communityId = rows[0][1].as<std::int64_t>();
    const std::string communityOwner = rows[0][2].as<std::string>();
    if (requesterLogin != channelOwner && requesterLogin != communityOwner &&
        !isModerator(transaction, communityId, requesterLogin)) {
        return MutationResult::kForbidden;
    }

    // messages cascade via ON DELETE CASCADE (see db/init.sql).
    transaction.exec("DELETE FROM channels WHERE id = $1", pqxx::params{id});
    transaction.commit();
    return MutationResult::kSuccess;
}

bool ChatRepository::joinCommunity(std::int64_t communityId, const std::string& login) {
    pqxx::connection connection(connectionString_);
    pqxx::work transaction(connection);

    try {
        transaction.exec("INSERT INTO memberships (community_id, member_login) VALUES ($1, $2) "
                          "ON CONFLICT (community_id, member_login) DO NOTHING",
                          pqxx::params{communityId, login});
        transaction.commit();
        return true;
    } catch (const pqxx::foreign_key_violation&) {
        return false;
    }
}

MutationResult ChatRepository::promoteModerator(std::int64_t communityId, const std::string& targetLogin,
                                                 const std::string& requesterLogin) {
    pqxx::connection connection(connectionString_);
    pqxx::work transaction(connection);

    const pqxx::result ownerRows =
        transaction.exec("SELECT owner_login FROM communities WHERE id = $1", pqxx::params{communityId});
    if (ownerRows.empty()) {
        return MutationResult::kNotFound;
    }
    if (ownerRows[0][0].as<std::string>() != requesterLogin) {
        return MutationResult::kForbidden;
    }

    transaction.exec(
        "INSERT INTO memberships (community_id, member_login, is_moderator) VALUES ($1, $2, TRUE) "
        "ON CONFLICT (community_id, member_login) DO UPDATE SET is_moderator = TRUE",
        pqxx::params{communityId, targetLogin});
    transaction.commit();
    return MutationResult::kSuccess;
}

MutationResult ChatRepository::demoteModerator(std::int64_t communityId, const std::string& targetLogin,
                                                const std::string& requesterLogin) {
    pqxx::connection connection(connectionString_);
    pqxx::work transaction(connection);

    const pqxx::result ownerRows =
        transaction.exec("SELECT owner_login FROM communities WHERE id = $1", pqxx::params{communityId});
    if (ownerRows.empty()) {
        return MutationResult::kNotFound;
    }
    if (ownerRows[0][0].as<std::string>() != requesterLogin) {
        return MutationResult::kForbidden;
    }

    // A no-op UPDATE (targetLogin was never a member/moderator) is still
    // kSuccess — see this method's doc comment.
    transaction.exec("UPDATE memberships SET is_moderator = FALSE WHERE community_id = $1 AND member_login = $2",
                      pqxx::params{communityId, targetLogin});
    transaction.commit();
    return MutationResult::kSuccess;
}

std::vector<std::string> ChatRepository::listModerators(std::int64_t communityId) {
    pqxx::connection connection(connectionString_);
    pqxx::work transaction(connection);

    const pqxx::result rows =
        transaction.exec("SELECT member_login FROM memberships WHERE community_id = $1 AND is_moderator ORDER BY member_login",
                          pqxx::params{communityId});

    std::vector<std::string> moderators;
    moderators.reserve(static_cast<std::size_t>(rows.size()));
    for (const auto& row : rows) {
        moderators.push_back(row[0].as<std::string>());
    }
    return moderators;
}

std::optional<Message> ChatRepository::insertMessage(std::int64_t channelId, const std::string& authorLogin,
                                                       const std::string& body) {
    pqxx::connection connection(connectionString_);
    pqxx::work transaction(connection);

    try {
        const pqxx::result rows = transaction.exec(
            "INSERT INTO messages (channel_id, author_login, body) VALUES ($1, $2, $3) "
            "RETURNING id, sent_at",
            pqxx::params{channelId, authorLogin, body});
        transaction.commit();
        return Message{.id = rows[0][0].as<std::int64_t>(),
                        .authorLogin = authorLogin,
                        .body = body,
                        .sentAt = rows[0][1].as<std::string>()};
    } catch (const pqxx::foreign_key_violation&) {
        return std::nullopt;
    }
}

std::vector<Message> ChatRepository::listRecentMessages(std::int64_t channelId, int limit,
                                                          std::optional<std::int64_t> beforeId) {
    pqxx::connection connection(connectionString_);
    pqxx::work transaction(connection);

    // Newest-first LIMIT, then reversed below, so the result is
    // chronological (oldest to newest) like a normal message log.
    // beforeId unset binds SQL NULL, matched by the IS NULL branch
    // (page ends at the newest message); set, it pages backward
    // through history (only messages older than that id). id DESC as
    // a secondary sort key makes the cursor unambiguous even if two
    // messages share the same sent_at.
    const pqxx::result rows = transaction.exec(
        "SELECT id, author_login, body, sent_at, edited_at FROM messages WHERE channel_id = $1 "
        "AND ($3::bigint IS NULL OR id < $3) ORDER BY sent_at DESC, id DESC LIMIT $2",
        pqxx::params{channelId, limit, beforeId});

    std::vector<Message> messages;
    messages.reserve(static_cast<std::size_t>(rows.size()));
    for (const auto& row : rows) {
        messages.push_back(
            Message{.id = row[0].as<std::int64_t>(),
                    .authorLogin = row[1].as<std::string>(),
                    .body = row[2].as<std::string>(),
                    .sentAt = row[3].as<std::string>(),
                    .editedAt = row[4].is_null() ? std::nullopt : std::make_optional(row[4].as<std::string>())});
    }
    std::reverse(messages.begin(), messages.end());
    return messages;
}

EditMessageResult ChatRepository::editMessage(std::int64_t messageId, std::int64_t channelId,
                                               const std::string& requesterLogin, const std::string& newBody) {
    pqxx::connection connection(connectionString_);
    pqxx::work transaction(connection);

    const pqxx::result authorRows = transaction.exec(
        "SELECT author_login FROM messages WHERE id = $1 AND channel_id = $2", pqxx::params{messageId, channelId});
    if (authorRows.empty()) {
        return EditMessageResult{.result = MutationResult::kNotFound};
    }
    if (authorRows[0][0].as<std::string>() != requesterLogin) {
        return EditMessageResult{.result = MutationResult::kForbidden};
    }

    const pqxx::result updated =
        transaction.exec("UPDATE messages SET body = $1, edited_at = now() WHERE id = $2 RETURNING edited_at",
                          pqxx::params{newBody, messageId});
    transaction.commit();
    return EditMessageResult{.result = MutationResult::kSuccess, .editedAt = updated[0][0].as<std::string>()};
}

MutationResult ChatRepository::deleteMessage(std::int64_t messageId, std::int64_t channelId,
                                              const std::string& requesterLogin) {
    pqxx::connection connection(connectionString_);
    pqxx::work transaction(connection);

    const pqxx::result rows = transaction.exec(
        "SELECT m.author_login, ch.owner_login, ch.community_id, co.owner_login FROM messages m "
        "JOIN channels ch ON ch.id = m.channel_id JOIN communities co ON co.id = ch.community_id "
        "WHERE m.id = $1 AND m.channel_id = $2",
        pqxx::params{messageId, channelId});
    if (rows.empty()) {
        return MutationResult::kNotFound;
    }
    const std::string authorLogin = rows[0][0].as<std::string>();
    const std::string channelOwner = rows[0][1].as<std::string>();
    const auto communityId = rows[0][2].as<std::int64_t>();
    const std::string communityOwner = rows[0][3].as<std::string>();
    if (requesterLogin != authorLogin && requesterLogin != channelOwner && requesterLogin != communityOwner &&
        !isModerator(transaction, communityId, requesterLogin)) {
        return MutationResult::kForbidden;
    }

    transaction.exec("DELETE FROM messages WHERE id = $1", pqxx::params{messageId});
    transaction.commit();
    return MutationResult::kSuccess;
}

}  // namespace chat_service
