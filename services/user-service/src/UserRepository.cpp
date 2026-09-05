#include "UserRepository.h"

#include <pqxx/pqxx>

#include <string_view>
#include <utility>

namespace user_service {

namespace {
/// friendships хранит ненаправленную пару в каноническом порядке
/// (меньший login первым) — одна строка вместо двух.
std::pair<std::string, std::string> canonicalPair(const std::string& loginA, const std::string& loginB) {
    return loginA < loginB ? std::make_pair(loginA, loginB) : std::make_pair(loginB, loginA);
}
}  // namespace

UserRepository::UserRepository(std::string connectionString) : connectionString_(std::move(connectionString)) {}

bool UserRepository::createUser(const std::string& login, const std::string& passwordHash) {
    pqxx::connection connection(connectionString_);
    pqxx::work transaction(connection);

    try {
        transaction.exec("INSERT INTO users (login, password_hash) VALUES ($1, $2)", pqxx::params{login, passwordHash});
        transaction.commit();
        return true;
    } catch (const pqxx::unique_violation&) {
        return false;
    }
}

std::optional<std::string> UserRepository::findPasswordHash(const std::string& login) {
    pqxx::connection connection(connectionString_);
    pqxx::work transaction(connection);

    const pqxx::result rows = transaction.exec("SELECT password_hash FROM users WHERE login = $1", pqxx::params{login});
    if (rows.empty()) {
        return std::nullopt;
    }

    return rows[0][0].as<std::string>();
}

std::optional<Profile> UserRepository::findProfile(const std::string& login) {
    pqxx::connection connection(connectionString_);
    pqxx::work transaction(connection);

    const pqxx::result rows = transaction.exec(
        "SELECT display_name, avatar_url, public_key, email, telegram_chat_id FROM users WHERE login = $1",
        pqxx::params{login});
    if (rows.empty()) {
        return std::nullopt;
    }

    return Profile{.login = login,
                    .displayName = rows[0][0].is_null() ? std::nullopt : std::make_optional(rows[0][0].as<std::string>()),
                    .avatarUrl = rows[0][1].is_null() ? std::nullopt : std::make_optional(rows[0][1].as<std::string>()),
                    .publicKey = rows[0][2].is_null() ? std::nullopt : std::make_optional(rows[0][2].as<std::string>()),
                    .email = rows[0][3].is_null() ? std::nullopt : std::make_optional(rows[0][3].as<std::string>()),
                    .telegramChatId =
                        rows[0][4].is_null() ? std::nullopt : std::make_optional(rows[0][4].as<std::string>())};
}

UpdateProfileResult UserRepository::updateProfile(const std::string& login, const ProfileUpdate& update) {
    pqxx::connection connection(connectionString_);
    pqxx::work transaction(connection);

    pqxx::result rows;
    try {
        rows = transaction.exec(
            "UPDATE users SET display_name = $1, avatar_url = $2, public_key = $3, email = $4, "
            "telegram_chat_id = $5 WHERE login = $6 RETURNING id",
            pqxx::params{update.displayName, update.avatarUrl, update.publicKey, update.email,
                         update.telegramChatId, login});
    } catch (const pqxx::unique_violation& error) {
        // libpqxx не выделяет имя нарушенного constraint'а отдельным
        // полем — Postgres кладёт его в текст ошибки
        // ("duplicate key value violates unique constraint
        // \"users_telegram_chat_id_unique\""), и .what() — единственный
        // практичный способ различить, какое из двух ограничений
        // сработало, без отдельного SELECT до UPDATE (что было бы race
        // condition-подвержено).
        const std::string_view message = error.what();
        return message.find("telegram_chat_id") != std::string_view::npos ? UpdateProfileResult::kTelegramChatIdTaken
                                                                            : UpdateProfileResult::kEmailTaken;
    }
    transaction.commit();
    return rows.empty() ? UpdateProfileResult::kNoSuchUser : UpdateProfileResult::kUpdated;
}

std::optional<OtpIdentity> UserRepository::resolveOtpIdentifier(const std::string& identifier) {
    pqxx::connection connection(connectionString_);
    pqxx::work transaction(connection);

    const pqxx::result rows =
        transaction.exec("SELECT login, email, telegram_chat_id FROM users WHERE login = $1 OR email = $1 OR "
                          "telegram_chat_id = $1",
                          pqxx::params{identifier});
    if (rows.empty() || (rows[0][1].is_null() && rows[0][2].is_null())) {
        return std::nullopt;
    }

    return OtpIdentity{
        .login = rows[0][0].as<std::string>(),
        .email = rows[0][1].is_null() ? std::nullopt : std::make_optional(rows[0][1].as<std::string>()),
        .telegramChatId = rows[0][2].is_null() ? std::nullopt : std::make_optional(rows[0][2].as<std::string>())};
}

SendFriendRequestResult UserRepository::sendFriendRequest(const std::string& requesterLogin,
                                                           const std::string& recipientLogin) {
    if (requesterLogin == recipientLogin) {
        return SendFriendRequestResult::kCannotFriendSelf;
    }

    pqxx::connection connection(connectionString_);
    pqxx::work transaction(connection);

    const pqxx::result recipientRows =
        transaction.exec("SELECT 1 FROM users WHERE login = $1", pqxx::params{recipientLogin});
    if (recipientRows.empty()) {
        return SendFriendRequestResult::kNoSuchRecipient;
    }

    const auto [loginA, loginB] = canonicalPair(requesterLogin, recipientLogin);
    const pqxx::result friendshipRows = transaction.exec(
        "SELECT 1 FROM friendships WHERE user_a_login = $1 AND user_b_login = $2", pqxx::params{loginA, loginB});
    if (!friendshipRows.empty()) {
        return SendFriendRequestResult::kAlreadyFriends;
    }

    // Взаимная заявка — получатель уже отправлял pending-заявку
    // отправителю — сразу становится дружбой, а не второй записью.
    const pqxx::result reverseRows = transaction.exec(
        "SELECT id FROM friend_requests WHERE requester_login = $1 AND recipient_login = $2 AND status = 'pending'",
        pqxx::params{recipientLogin, requesterLogin});
    if (!reverseRows.empty()) {
        transaction.exec("UPDATE friend_requests SET status = 'accepted', responded_at = now() WHERE id = $1",
                          pqxx::params{reverseRows[0][0].as<std::int64_t>()});
        transaction.exec("INSERT INTO friendships (user_a_login, user_b_login) VALUES ($1, $2)",
                          pqxx::params{loginA, loginB});
        transaction.commit();
        return SendFriendRequestResult::kAutoAccepted;
    }

    try {
        transaction.exec("INSERT INTO friend_requests (requester_login, recipient_login) VALUES ($1, $2)",
                          pqxx::params{requesterLogin, recipientLogin});
    } catch (const pqxx::unique_violation&) {
        // Частичный уникальный индекс friend_requests_pending_unique —
        // уже есть pending-заявка в этом же направлении.
        return SendFriendRequestResult::kAlreadyRequested;
    }
    transaction.commit();
    return SendFriendRequestResult::kSent;
}

RespondToFriendRequestResult UserRepository::respondToFriendRequest(std::int64_t requestId,
                                                                      const std::string& recipientLogin,
                                                                      bool accept) {
    pqxx::connection connection(connectionString_);
    pqxx::work transaction(connection);

    const pqxx::result rows = transaction.exec(
        "SELECT requester_login, recipient_login FROM friend_requests WHERE id = $1 AND status = 'pending'",
        pqxx::params{requestId});
    if (rows.empty()) {
        return RespondToFriendRequestResult::kNoSuchRequest;
    }
    const std::string requesterLogin = rows[0][0].as<std::string>();
    if (rows[0][1].as<std::string>() != recipientLogin) {
        return RespondToFriendRequestResult::kNotYourRequest;
    }

    transaction.exec("UPDATE friend_requests SET status = $1, responded_at = now() WHERE id = $2",
                      pqxx::params{std::string(accept ? "accepted" : "declined"), requestId});
    if (accept) {
        const auto [loginA, loginB] = canonicalPair(requesterLogin, recipientLogin);
        transaction.exec("INSERT INTO friendships (user_a_login, user_b_login) VALUES ($1, $2)",
                          pqxx::params{loginA, loginB});
    }
    transaction.commit();
    return accept ? RespondToFriendRequestResult::kAccepted : RespondToFriendRequestResult::kDeclined;
}

std::vector<FriendRequestInfo> UserRepository::listIncomingFriendRequests(const std::string& login) {
    pqxx::connection connection(connectionString_);
    pqxx::work transaction(connection);

    const pqxx::result rows =
        transaction.exec("SELECT id, requester_login, created_at FROM friend_requests "
                          "WHERE recipient_login = $1 AND status = 'pending' ORDER BY created_at DESC",
                          pqxx::params{login});

    std::vector<FriendRequestInfo> requests;
    requests.reserve(rows.size());
    for (const auto& row : rows) {
        requests.push_back(FriendRequestInfo{.id = row[0].as<std::int64_t>(),
                                              .requesterLogin = row[1].as<std::string>(),
                                              .createdAt = row[2].as<std::string>()});
    }
    return requests;
}

std::vector<std::string> UserRepository::listFriends(const std::string& login) {
    pqxx::connection connection(connectionString_);
    pqxx::work transaction(connection);

    const pqxx::result rows =
        transaction.exec("SELECT CASE WHEN user_a_login = $1 THEN user_b_login ELSE user_a_login END "
                          "FROM friendships WHERE user_a_login = $1 OR user_b_login = $1",
                          pqxx::params{login});

    std::vector<std::string> friends;
    friends.reserve(rows.size());
    for (const auto& row : rows) {
        friends.push_back(row[0].as<std::string>());
    }
    return friends;
}

bool UserRepository::removeFriend(const std::string& loginA, const std::string& loginB) {
    pqxx::connection connection(connectionString_);
    pqxx::work transaction(connection);

    const auto [canonicalA, canonicalB] = canonicalPair(loginA, loginB);
    const pqxx::result rows =
        transaction.exec("DELETE FROM friendships WHERE user_a_login = $1 AND user_b_login = $2 RETURNING 1",
                          pqxx::params{canonicalA, canonicalB});
    transaction.commit();
    return !rows.empty();
}

bool UserRepository::areFriends(const std::string& loginA, const std::string& loginB) {
    pqxx::connection connection(connectionString_);
    pqxx::work transaction(connection);

    const auto [canonicalA, canonicalB] = canonicalPair(loginA, loginB);
    const pqxx::result rows = transaction.exec(
        "SELECT 1 FROM friendships WHERE user_a_login = $1 AND user_b_login = $2", pqxx::params{canonicalA, canonicalB});
    return !rows.empty();
}

}  // namespace user_service
