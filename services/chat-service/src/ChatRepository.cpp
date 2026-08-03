#include "ChatRepository.h"

#include <pqxx/pqxx>

#include <algorithm>

namespace chat_service {

ChatRepository::ChatRepository(std::string connectionString) : connectionString_(std::move(connectionString)) {}

Community ChatRepository::createCommunity(const std::string& name) {
    pqxx::connection connection(connectionString_);
    pqxx::work transaction(connection);

    const pqxx::result rows =
        transaction.exec("INSERT INTO communities (name) VALUES ($1) RETURNING id", pqxx::params{name});
    transaction.commit();

    return Community{.id = rows[0][0].as<std::int64_t>(), .name = name};
}

std::vector<Community> ChatRepository::listCommunities() {
    pqxx::connection connection(connectionString_);
    pqxx::work transaction(connection);

    const pqxx::result rows = transaction.exec("SELECT id, name FROM communities ORDER BY id");

    std::vector<Community> communities;
    communities.reserve(static_cast<std::size_t>(rows.size()));
    for (const auto& row : rows) {
        communities.push_back(Community{.id = row[0].as<std::int64_t>(), .name = row[1].as<std::string>()});
    }
    return communities;
}

std::optional<std::int64_t> ChatRepository::createChannel(std::int64_t communityId, const std::string& name) {
    pqxx::connection connection(connectionString_);
    pqxx::work transaction(connection);

    try {
        const pqxx::result rows =
            transaction.exec("INSERT INTO channels (community_id, name) VALUES ($1, $2) RETURNING id",
                              pqxx::params{communityId, name});
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

    const pqxx::result rows = transaction.exec("SELECT id, community_id, name FROM channels WHERE community_id = $1 "
                                                "ORDER BY id",
                                                pqxx::params{communityId});

    std::vector<Channel> channels;
    channels.reserve(static_cast<std::size_t>(rows.size()));
    for (const auto& row : rows) {
        channels.push_back(Channel{.id = row[0].as<std::int64_t>(),
                                    .communityId = row[1].as<std::int64_t>(),
                                    .name = row[2].as<std::string>()});
    }
    return channels;
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

std::vector<Message> ChatRepository::listRecentMessages(std::int64_t channelId, int limit) {
    pqxx::connection connection(connectionString_);
    pqxx::work transaction(connection);

    // Newest-first LIMIT, then reversed below, so the result is
    // chronological (oldest to newest) like a normal message log.
    const pqxx::result rows =
        transaction.exec("SELECT id, author_login, body, sent_at FROM messages WHERE channel_id = $1 "
                          "ORDER BY sent_at DESC LIMIT $2",
                          pqxx::params{channelId, limit});

    std::vector<Message> messages;
    messages.reserve(static_cast<std::size_t>(rows.size()));
    for (const auto& row : rows) {
        messages.push_back(Message{.id = row[0].as<std::int64_t>(),
                                    .authorLogin = row[1].as<std::string>(),
                                    .body = row[2].as<std::string>(),
                                    .sentAt = row[3].as<std::string>()});
    }
    std::reverse(messages.begin(), messages.end());
    return messages;
}

}  // namespace chat_service
