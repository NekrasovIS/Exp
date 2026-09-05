#include "ChatRepository.h"

#include <pqxx/pqxx>

#include <algorithm>
#include <utility>

namespace chat_service {

namespace {

/// @return True, если @p login — модератор @p communityId, в рамках уже
/// открытой @p transaction — используется совместно deleteMessage()/
/// renameChannel()/deleteChannel(), каждому из которых нужна та же
/// проверка наряду с собственной проверкой владельца.
bool isModerator(pqxx::work& transaction, std::int64_t communityId, const std::string& login) {
    const pqxx::result rows =
        transaction.exec("SELECT is_moderator FROM memberships WHERE community_id = $1 AND member_login = $2",
                          pqxx::params{communityId, login});
    return !rows.empty() && rows[0][0].as<bool>();
}

/// direct_message_threads хранит неупорядоченную пару в каноническом
/// порядке (меньший login первым) — одна строка вместо двух, тот же
/// приём, что и у friendships в user-service.
std::pair<std::string, std::string> canonicalPair(const std::string& loginA, const std::string& loginB) {
    return loginA < loginB ? std::make_pair(loginA, loginB) : std::make_pair(loginB, loginA);
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

    // channels/memberships/messages каскадно удаляются через ON DELETE CASCADE (см. db/init.sql).
    transaction.exec("DELETE FROM communities WHERE id = $1", pqxx::params{id});
    transaction.commit();
    return MutationResult::kSuccess;
}

std::optional<std::int64_t> ChatRepository::createChannel(std::int64_t communityId, const std::string& name,
                                                            const std::string& ownerLogin, bool isEncrypted) {
    pqxx::connection connection(connectionString_);
    pqxx::work transaction(connection);

    try {
        const pqxx::result rows = transaction.exec(
            "INSERT INTO channels (community_id, name, owner_login, is_encrypted) VALUES ($1, $2, $3, $4) "
            "RETURNING id",
            pqxx::params{communityId, name, ownerLogin, isEncrypted});
        transaction.commit();
        return rows[0][0].as<std::int64_t>();
    } catch (const pqxx::foreign_key_violation&) {
        return std::nullopt;
    } catch (const pqxx::unique_violation&) {
        return std::nullopt;
    }
}

namespace {
// Шаблон по типу строки, поскольку pqxx возвращает разные типы ссылок
// на строку при итерации range-for (pqxx::row_ref) и при индексированном
// доступе (pqxx::const_result_iterator::reference) — оба одинаково
// поддерживают operator[], поэтому шаблон избавляет от необходимости в
// двух почти идентичных перегрузках.
template <typename Row>
Channel channelFromRow(const Row& row) {
    return Channel{.id = row[0].template as<std::int64_t>(),
                   .communityId = row[1].template as<std::int64_t>(),
                   .name = row[2].template as<std::string>(),
                   .ownerLogin = row[3].template as<std::string>(),
                   .isEncrypted = row[4].template as<bool>()};
}
}  // namespace

std::vector<Channel> ChatRepository::listChannels(std::int64_t communityId) {
    pqxx::connection connection(connectionString_);
    pqxx::work transaction(connection);

    const pqxx::result rows = transaction.exec(
        "SELECT id, community_id, name, owner_login, is_encrypted FROM channels WHERE community_id = $1 "
        "ORDER BY id",
        pqxx::params{communityId});

    std::vector<Channel> channels;
    channels.reserve(static_cast<std::size_t>(rows.size()));
    for (const auto& row : rows) {
        channels.push_back(channelFromRow(row));
    }
    return channels;
}

std::optional<Channel> ChatRepository::findChannel(std::int64_t id) {
    pqxx::connection connection(connectionString_);
    pqxx::work transaction(connection);

    const pqxx::result rows = transaction.exec(
        "SELECT id, community_id, name, owner_login, is_encrypted FROM channels WHERE id = $1", pqxx::params{id});
    if (rows.empty()) {
        return std::nullopt;
    }
    return channelFromRow(rows[0]);
}

std::vector<std::string> ChatRepository::listMembers(std::int64_t communityId) {
    pqxx::connection connection(connectionString_);
    pqxx::work transaction(connection);

    const pqxx::result rows = transaction.exec(
        "SELECT member_login FROM memberships WHERE community_id = $1 ORDER BY member_login",
        pqxx::params{communityId});

    std::vector<std::string> members;
    members.reserve(static_cast<std::size_t>(rows.size()));
    for (const auto& row : rows) {
        members.push_back(row[0].as<std::string>());
    }
    return members;
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

    // messages каскадно удаляются через ON DELETE CASCADE (см. db/init.sql).
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

    // UPDATE без эффекта (targetLogin никогда не был участником/
    // модератором) всё равно даёт kSuccess — см. doc-комментарий этого метода.
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
                                                       const std::string& body,
                                                       std::optional<std::int64_t> attachmentId) {
    pqxx::connection connection(connectionString_);
    pqxx::work transaction(connection);

    try {
        const pqxx::result rows = transaction.exec(
            "INSERT INTO messages (channel_id, author_login, body, attachment_id) VALUES ($1, $2, $3, $4) "
            "RETURNING id, sent_at",
            pqxx::params{channelId, authorLogin, body, attachmentId});

        std::optional<std::string> attachmentFilename;
        if (attachmentId.has_value()) {
            const pqxx::result attachmentRows =
                transaction.exec("SELECT filename FROM attachments WHERE id = $1", pqxx::params{*attachmentId});
            if (!attachmentRows.empty()) {
                attachmentFilename = attachmentRows[0][0].as<std::string>();
            }
        }

        transaction.commit();
        return Message{.id = rows[0][0].as<std::int64_t>(),
                        .authorLogin = authorLogin,
                        .body = body,
                        .sentAt = rows[0][1].as<std::string>(),
                        .attachmentId = attachmentId,
                        .attachmentFilename = attachmentFilename};
    } catch (const pqxx::foreign_key_violation&) {
        // Либо channelId, либо attachmentId (если установлен) не существует.
        return std::nullopt;
    }
}

std::vector<Message> ChatRepository::listRecentMessages(std::int64_t channelId, int limit,
                                                          std::optional<std::int64_t> beforeId) {
    pqxx::connection connection(connectionString_);
    pqxx::work transaction(connection);

    // LIMIT сначала по новейшим, затем разворачивается ниже, чтобы
    // результат был хронологическим (от старых к новым), как в обычном
    // логе сообщений. Неустановленный beforeId связывается с SQL NULL,
    // что соответствует ветке IS NULL (страница заканчивается самым
    // новым сообщением); установленный — листает историю назад (только
    // сообщения старше этого id). id DESC как вторичный ключ сортировки
    // делает курсор однозначным, даже если у двух сообщений совпадает sent_at.
    const pqxx::result rows = transaction.exec(
        "SELECT m.id, m.author_login, m.body, m.sent_at, m.edited_at, m.attachment_id, a.filename "
        "FROM messages m LEFT JOIN attachments a ON a.id = m.attachment_id "
        "WHERE m.channel_id = $1 AND ($3::bigint IS NULL OR m.id < $3) "
        "ORDER BY m.sent_at DESC, m.id DESC LIMIT $2",
        pqxx::params{channelId, limit, beforeId});

    std::vector<Message> messages;
    messages.reserve(static_cast<std::size_t>(rows.size()));
    for (const auto& row : rows) {
        messages.push_back(
            Message{.id = row[0].as<std::int64_t>(),
                    .authorLogin = row[1].as<std::string>(),
                    .body = row[2].as<std::string>(),
                    .sentAt = row[3].as<std::string>(),
                    .editedAt = row[4].is_null() ? std::nullopt : std::make_optional(row[4].as<std::string>()),
                    .attachmentId = row[5].is_null() ? std::nullopt : std::make_optional(row[5].as<std::int64_t>()),
                    .attachmentFilename =
                        row[6].is_null() ? std::nullopt : std::make_optional(row[6].as<std::string>())});
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

std::optional<AttachmentMetadata> ChatRepository::createAttachment(std::int64_t channelId,
                                                                     const AttachmentUpload& upload) {
    pqxx::connection connection(connectionString_);
    pqxx::work transaction(connection);

    try {
        const pqxx::result rows = transaction.exec(
            "INSERT INTO attachments (channel_id, uploader_login, filename, content_type, data_base64, size_bytes) "
            "VALUES ($1, $2, $3, $4, $5, $6) RETURNING id",
            pqxx::params{channelId, upload.uploaderLogin, upload.filename, upload.contentType, upload.dataBase64,
                         upload.sizeBytes});
        transaction.commit();
        return AttachmentMetadata{.id = rows[0][0].as<std::int64_t>(),
                                   .filename = upload.filename,
                                   .contentType = upload.contentType,
                                   .sizeBytes = upload.sizeBytes};
    } catch (const pqxx::foreign_key_violation&) {
        return std::nullopt;
    }
}

std::optional<AttachmentData> ChatRepository::findAttachmentData(std::int64_t attachmentId) {
    pqxx::connection connection(connectionString_);
    pqxx::work transaction(connection);

    const pqxx::result rows = transaction.exec(
        "SELECT filename, content_type, data_base64 FROM attachments WHERE id = $1", pqxx::params{attachmentId});
    if (rows.empty()) {
        return std::nullopt;
    }

    return AttachmentData{.filename = rows[0][0].as<std::string>(),
                           .contentType = rows[0][1].as<std::string>(),
                           .data = rows[0][2].as<std::string>()};
}

std::vector<Message> ChatRepository::searchMessages(std::int64_t channelId, const std::string& query, int limit) {
    pqxx::connection connection(connectionString_);
    pqxx::work transaction(connection);

    // position(...) > 0 вместо ILIKE — обычная регистронезависимая
    // проверка подстроки, поэтому символы вроде '%'/'_' в запросе
    // сопоставляются буквально, а не трактуются как SQL-подстановочные знаки.
    const pqxx::result rows = transaction.exec(
        "SELECT m.id, m.author_login, m.body, m.sent_at, m.edited_at, m.attachment_id, a.filename "
        "FROM messages m LEFT JOIN attachments a ON a.id = m.attachment_id "
        "WHERE m.channel_id = $1 AND position(lower($2) in lower(m.body)) > 0 "
        "ORDER BY m.sent_at DESC, m.id DESC LIMIT $3",
        pqxx::params{channelId, query, limit});

    std::vector<Message> messages;
    messages.reserve(static_cast<std::size_t>(rows.size()));
    for (const auto& row : rows) {
        messages.push_back(
            Message{.id = row[0].as<std::int64_t>(),
                    .authorLogin = row[1].as<std::string>(),
                    .body = row[2].as<std::string>(),
                    .sentAt = row[3].as<std::string>(),
                    .editedAt = row[4].is_null() ? std::nullopt : std::make_optional(row[4].as<std::string>()),
                    .attachmentId = row[5].is_null() ? std::nullopt : std::make_optional(row[5].as<std::int64_t>()),
                    .attachmentFilename =
                        row[6].is_null() ? std::nullopt : std::make_optional(row[6].as<std::string>())});
    }
    return messages;
}

MutationResult ChatRepository::setChannelKey(std::int64_t channelId, const std::string& memberLogin,
                                              const std::string& requesterLogin, const std::string& wrappedKey) {
    pqxx::connection connection(connectionString_);
    pqxx::work transaction(connection);

    const pqxx::result rows = transaction.exec(
        "SELECT ch.owner_login, ch.community_id, co.owner_login FROM channels ch "
        "JOIN communities co ON co.id = ch.community_id WHERE ch.id = $1",
        pqxx::params{channelId});
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

    transaction.exec(
        "INSERT INTO channel_keys (channel_id, member_login, wrapped_key) VALUES ($1, $2, $3) "
        "ON CONFLICT (channel_id, member_login) DO UPDATE SET wrapped_key = EXCLUDED.wrapped_key",
        pqxx::params{channelId, memberLogin, wrappedKey});
    transaction.commit();
    return MutationResult::kSuccess;
}

std::optional<std::string> ChatRepository::findChannelKey(std::int64_t channelId, const std::string& login) {
    pqxx::connection connection(connectionString_);
    pqxx::work transaction(connection);

    const pqxx::result rows =
        transaction.exec("SELECT wrapped_key FROM channel_keys WHERE channel_id = $1 AND member_login = $2",
                          pqxx::params{channelId, login});
    if (rows.empty()) {
        return std::nullopt;
    }
    return rows[0][0].as<std::string>();
}

std::int64_t ChatRepository::findOrCreateThread(const std::string& loginA, const std::string& loginB) {
    pqxx::connection connection(connectionString_);
    pqxx::work transaction(connection);

    const auto [canonicalA, canonicalB] = canonicalPair(loginA, loginB);
    // INSERT ... ON CONFLICT DO NOTHING, затем SELECT, а не наоборот —
    // избегает состояния гонки между проверкой существования и
    // созданием при двух одновременных первых сообщениях одной паре.
    transaction.exec(
        "INSERT INTO direct_message_threads (user_a_login, user_b_login) VALUES ($1, $2) "
        "ON CONFLICT (user_a_login, user_b_login) DO NOTHING",
        pqxx::params{canonicalA, canonicalB});
    const pqxx::result rows = transaction.exec(
        "SELECT id FROM direct_message_threads WHERE user_a_login = $1 AND user_b_login = $2",
        pqxx::params{canonicalA, canonicalB});
    transaction.commit();
    return rows[0][0].as<std::int64_t>();
}

std::vector<DirectMessageThread> ChatRepository::listMyThreads(const std::string& login) {
    pqxx::connection connection(connectionString_);
    pqxx::work transaction(connection);

    const pqxx::result rows = transaction.exec(
        "SELECT id, CASE WHEN user_a_login = $1 THEN user_b_login ELSE user_a_login END, created_at "
        "FROM direct_message_threads WHERE user_a_login = $1 OR user_b_login = $1 ORDER BY created_at DESC",
        pqxx::params{login});

    std::vector<DirectMessageThread> threads;
    threads.reserve(rows.size());
    for (const auto& row : rows) {
        threads.push_back(DirectMessageThread{
            .id = row[0].as<std::int64_t>(), .otherLogin = row[1].as<std::string>(), .createdAt = row[2].as<std::string>()});
    }
    return threads;
}

bool ChatRepository::isThreadParticipant(std::int64_t threadId, const std::string& login) {
    pqxx::connection connection(connectionString_);
    pqxx::work transaction(connection);

    const pqxx::result rows =
        transaction.exec("SELECT 1 FROM direct_message_threads WHERE id = $1 AND (user_a_login = $2 OR user_b_login = $2)",
                          pqxx::params{threadId, login});
    return !rows.empty();
}

std::optional<DirectMessage> ChatRepository::insertDirectMessage(std::int64_t threadId, const std::string& authorLogin,
                                                                   const std::string& body) {
    pqxx::connection connection(connectionString_);
    pqxx::work transaction(connection);

    const pqxx::result rows =
        transaction.exec("INSERT INTO direct_messages (thread_id, author_login, body) "
                          "SELECT $1, $2, $3 WHERE EXISTS (SELECT 1 FROM direct_message_threads WHERE id = $1) "
                          "RETURNING id, sent_at",
                          pqxx::params{threadId, authorLogin, body});
    if (rows.empty()) {
        return std::nullopt;
    }
    transaction.commit();
    return DirectMessage{.id = rows[0][0].as<std::int64_t>(),
                         .authorLogin = authorLogin,
                         .body = body,
                         .sentAt = rows[0][1].as<std::string>()};
}

std::vector<DirectMessage> ChatRepository::listDirectMessages(std::int64_t threadId, int limit,
                                                                std::optional<std::int64_t> beforeId) {
    pqxx::connection connection(connectionString_);
    pqxx::work transaction(connection);

    // Тот же приём "LIMIT по новейшим, затем развернуть", что и у
    // listRecentMessages() — см. её doc-комментарий.
    const pqxx::result rows = transaction.exec(
        "SELECT id, author_login, body, sent_at FROM direct_messages "
        "WHERE thread_id = $1 AND ($3::bigint IS NULL OR id < $3) "
        "ORDER BY sent_at DESC, id DESC LIMIT $2",
        pqxx::params{threadId, limit, beforeId});

    std::vector<DirectMessage> messages;
    messages.reserve(rows.size());
    for (const auto& row : rows) {
        messages.push_back(DirectMessage{.id = row[0].as<std::int64_t>(),
                                          .authorLogin = row[1].as<std::string>(),
                                          .body = row[2].as<std::string>(),
                                          .sentAt = row[3].as<std::string>()});
    }
    std::reverse(messages.begin(), messages.end());
    return messages;
}

}  // namespace chat_service
