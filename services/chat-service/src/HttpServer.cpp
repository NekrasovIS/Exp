#include "HttpServer.h"

#include <nlohmann/json.hpp>

#include <string_view>

#include "JsonGuard.h"
#include "base64.h"

namespace chat_service {

namespace {
constexpr const char* kJsonContentType = "application/json";
constexpr std::string_view kBearerPrefix = "Bearer ";
constexpr int kDefaultMessageLimit = 50;
/// Применяется к количеству байт *после декодирования*, а не к
/// передаваемому по проводу base64-payload (issue #116) — см.
/// doc-комментарий класса ChatRepository о том, почему вложения
/// хранятся как base64 TEXT, а не как BYTEA/на диске.
constexpr std::size_t kMaxAttachmentSizeBytes = 5 * 1024 * 1024;
/// Issue #226 (pentest) — см. doc-комментарий у проверки в
/// handleUploadAttachment().
constexpr std::size_t kMaxAttachmentFilenameLength = 255;
constexpr int kDefaultSearchLimit = 20;

// Имена файлов вложений приходят от недоверенного клиента (issue #116)
// и встраиваются дословно в заголовок ответа Content-Disposition —
// перед этим вырезает '"' (иначе позволило бы вырваться из значения
// имени файла в кавычках) и все управляющие символы ASCII (0x00-0x1F,
// включая CR/LF — инъекция типа header/response-splitting, issue #226
// pentest: сюда же относятся и прочие control-символы, не только
// CR/LF, на случай менее строгих клиентских парсеров заголовков).
std::string sanitizeForHeaderValue(const std::string& value) {
    std::string sanitized;
    sanitized.reserve(value.size());
    for (const char c : value) {
        if (static_cast<unsigned char>(c) >= 0x20 && c != '"') {
            sanitized += c;
        }
    }
    return sanitized;
}

/// @p includeInviteCode — только для ответов, где вызывающая сторона уже
/// имеет право знать код (создатель сразу после создания, участник в
/// GET /communities/mine, issue #186) — не для устаревшего
/// GET /communities (см. handleListCommunities()), который отдаёт все
/// сообщества без проверки членства.
nlohmann::json toJson(const Community& community, bool includeInviteCode) {
    nlohmann::json json{{"id", community.id}, {"name", community.name}, {"owner", community.ownerLogin}};
    if (includeInviteCode) {
        json["invite_code"] =
            community.inviteCode.has_value() ? nlohmann::json(*community.inviteCode) : nlohmann::json(nullptr);
    }
    return json;
}

nlohmann::json toJson(const Channel& channel) {
    return nlohmann::json{{"id", channel.id},
                           {"community_id", channel.communityId},
                           {"name", channel.name},
                           {"owner", channel.ownerLogin},
                           {"is_encrypted", channel.isEncrypted}};
}

nlohmann::json toJson(const MessageReaction& reaction) {
    return nlohmann::json{{"emoji", reaction.emoji}, {"logins", reaction.logins}};
}

nlohmann::json toJson(const Message& message) {
    nlohmann::json reactions = nlohmann::json::array();
    for (const MessageReaction& reaction : message.reactions) {
        reactions.push_back(toJson(reaction));
    }
    return nlohmann::json{
        {"id", message.id},
        {"author", message.authorLogin},
        {"body", message.body},
        {"sent_at", message.sentAt},
        {"edited_at", message.editedAt.has_value() ? nlohmann::json(*message.editedAt) : nlohmann::json(nullptr)},
        {"attachment_id",
         message.attachmentId.has_value() ? nlohmann::json(*message.attachmentId) : nlohmann::json(nullptr)},
        {"attachment_filename", message.attachmentFilename.has_value() ? nlohmann::json(*message.attachmentFilename)
                                                                        : nlohmann::json(nullptr)},
        {"reactions", reactions},
        {"reply_to_message_id", message.replyToMessageId.has_value() ? nlohmann::json(*message.replyToMessageId)
                                                                       : nlohmann::json(nullptr)}};
}

nlohmann::json toJson(const DirectMessageThread& thread) {
    return nlohmann::json{{"id", thread.id}, {"other_login", thread.otherLogin}, {"created_at", thread.createdAt}};
}

nlohmann::json toJson(const DirectMessage& message) {
    return nlohmann::json{{"id", message.id}, {"author", message.authorLogin}, {"body", message.body}, {"sent_at", message.sentAt}};
}

std::optional<std::string> parseRecipientLogin(const std::string& body) {
    if (json_guard::exceedsMaxNestingDepth(body, json_guard::kMaxNestingDepth)) {
        return std::nullopt;
    }
    const nlohmann::json json = nlohmann::json::parse(body, nullptr, /*allow_exceptions=*/false);
    if (json.is_discarded() || !json.contains("recipient_login") || !json["recipient_login"].is_string()) {
        return std::nullopt;
    }
    return json["recipient_login"].get<std::string>();
}

std::optional<std::string> parseMessageBody(const std::string& body) {
    if (json_guard::exceedsMaxNestingDepth(body, json_guard::kMaxNestingDepth)) {
        return std::nullopt;
    }
    const nlohmann::json json = nlohmann::json::parse(body, nullptr, /*allow_exceptions=*/false);
    if (json.is_discarded() || !json.contains("body") || !json["body"].is_string()) {
        return std::nullopt;
    }
    return json["body"].get<std::string>();
}
}  // namespace

HttpServer::HttpServer(ChatService& chatService, const AuthServiceClient& authServiceClient,
                        const UserServiceClient& userServiceClient)
    : chatService_(chatService), authServiceClient_(authServiceClient), userServiceClient_(userServiceClient) {
    registerRoutes();
}

std::optional<std::string> HttpServer::authenticate(const httplib::Request& request) const {
    const std::string header = request.get_header_value("Authorization");
    if (header.size() <= kBearerPrefix.size() || header.compare(0, kBearerPrefix.size(), kBearerPrefix) != 0) {
        return std::nullopt;
    }
    return authServiceClient_.verifyToken(header.substr(kBearerPrefix.size()));
}

void HttpServer::registerRoutes() {
    server_.Post("/communities", [this](const httplib::Request& request, httplib::Response& response) {
        handleCreateCommunity(request, response);
    });
    server_.Get("/communities", [this](const httplib::Request& request, httplib::Response& response) {
        handleListCommunities(request, response);
    });
    // Issue #186 — регистрируется раньше /communities/(\d+)/... ниже,
    // иначе "mine" никогда бы не совпало с ними первым, но здесь это не
    // играет роли: "mine" — не число, ни один из \d+-маршрутов на него
    // и не претендует. Порядок сохранён для читаемости (рядом с
    // остальными /communities-маршрутами), а не по необходимости.
    server_.Get("/communities/mine", [this](const httplib::Request& request, httplib::Response& response) {
        handleListMyCommunities(request, response);
    });
    server_.Post("/communities/join-by-code",
                  [this](const httplib::Request& request, httplib::Response& response) {
                      handleJoinCommunityByCode(request, response);
                  });
    server_.Patch(R"(/communities/(\d+))", [this](const httplib::Request& request, httplib::Response& response) {
        handleRenameCommunity(request, response);
    });
    server_.Delete(R"(/communities/(\d+))", [this](const httplib::Request& request, httplib::Response& response) {
        handleDeleteCommunity(request, response);
    });
    server_.Post(R"(/communities/(\d+)/join)",
                  [this](const httplib::Request& request, httplib::Response& response) {
                      handleJoinCommunity(request, response);
                  });
    server_.Post(R"(/communities/(\d+)/invite/regenerate)",
                  [this](const httplib::Request& request, httplib::Response& response) {
                      handleRegenerateInviteCode(request, response);
                  });
    server_.Post(R"(/communities/(\d+)/channels)",
                  [this](const httplib::Request& request, httplib::Response& response) {
                      handleCreateChannel(request, response);
                  });
    server_.Get(R"(/communities/(\d+)/channels)",
                 [this](const httplib::Request& request, httplib::Response& response) {
                     handleListChannels(request, response);
                 });
    server_.Patch(R"(/channels/(\d+))", [this](const httplib::Request& request, httplib::Response& response) {
        handleRenameChannel(request, response);
    });
    server_.Delete(R"(/channels/(\d+))", [this](const httplib::Request& request, httplib::Response& response) {
        handleDeleteChannel(request, response);
    });
    server_.Get(R"(/channels/(\d+)/messages)",
                 [this](const httplib::Request& request, httplib::Response& response) {
                     handleListMessages(request, response);
                 });
    server_.Post(R"(/communities/(\d+)/moderators)",
                  [this](const httplib::Request& request, httplib::Response& response) {
                      handlePromoteModerator(request, response);
                  });
    server_.Delete(R"(/communities/(\d+)/moderators/([^/]+))",
                    [this](const httplib::Request& request, httplib::Response& response) {
                        handleDemoteModerator(request, response);
                    });
    server_.Get(R"(/communities/(\d+)/moderators)",
                 [this](const httplib::Request& request, httplib::Response& response) {
                     handleListModerators(request, response);
                 });
    server_.Post(R"(/channels/(\d+)/attachments)",
                  [this](const httplib::Request& request, httplib::Response& response) {
                      handleUploadAttachment(request, response);
                  });
    server_.Get(R"(/attachments/(\d+))", [this](const httplib::Request& request, httplib::Response& response) {
        handleDownloadAttachment(request, response);
    });
    server_.Get(R"(/channels/(\d+)/messages/search)",
                 [this](const httplib::Request& request, httplib::Response& response) {
                     handleSearchMessages(request, response);
                 });
    server_.Get(R"(/communities/(\d+)/members)",
                 [this](const httplib::Request& request, httplib::Response& response) {
                     handleListMembers(request, response);
                 });
    server_.Put(R"(/channels/(\d+)/keys/([^/]+))",
                 [this](const httplib::Request& request, httplib::Response& response) {
                     handleSetChannelKey(request, response);
                 });
    server_.Get(R"(/channels/(\d+)/keys/me)",
                 [this](const httplib::Request& request, httplib::Response& response) {
                     handleGetMyChannelKey(request, response);
                 });
    server_.Post("/dm/threads", [this](const httplib::Request& request, httplib::Response& response) {
        handleOpenThread(request, response);
    });
    server_.Get("/dm/threads", [this](const httplib::Request& request, httplib::Response& response) {
        handleListMyThreads(request, response);
    });
    server_.Post(R"(/dm/threads/(\d+)/messages)",
                  [this](const httplib::Request& request, httplib::Response& response) {
                      handlePostDirectMessage(request, response);
                  });
    server_.Get(R"(/dm/threads/(\d+)/messages)",
                 [this](const httplib::Request& request, httplib::Response& response) {
                     handleListDirectMessages(request, response);
                 });
}

void HttpServer::handleCreateCommunity(const httplib::Request& request, httplib::Response& response) {
    const std::optional<std::string> login = authenticate(request);
    if (!login.has_value()) {
        response.status = 401;
        return;
    }

    if (json_guard::exceedsMaxNestingDepth(request.body, json_guard::kMaxNestingDepth)) {
        response.status = 400;
        response.set_content(nlohmann::json{{"error", "payload too deeply nested"}}.dump(), kJsonContentType);
        return;
    }
    const nlohmann::json body = nlohmann::json::parse(request.body, nullptr, /*allow_exceptions=*/false);
    if (body.is_discarded() || !body.contains("name") || !body["name"].is_string()) {
        response.status = 400;
        response.set_content(nlohmann::json{{"error", "expected 'name' string"}}.dump(), kJsonContentType);
        return;
    }

    const Community community = chatService_.createCommunity(body["name"].get<std::string>(), *login);
    response.status = 201;
    // Создатель сразу видит код приглашения — ему и предстоит его
    // раздавать (issue #186).
    response.set_content(toJson(community, /*includeInviteCode=*/true).dump(), kJsonContentType);
}

void HttpServer::handleListCommunities(const httplib::Request& request, httplib::Response& response) {
    if (!authenticate(request).has_value()) {
        response.status = 401;
        return;
    }

    nlohmann::json communities = nlohmann::json::array();
    for (const Community& community : chatService_.listCommunities()) {
        communities.push_back(toJson(community, /*includeInviteCode=*/false));
    }
    response.set_content(communities.dump(), kJsonContentType);
}

void HttpServer::handleListMyCommunities(const httplib::Request& request, httplib::Response& response) {
    const std::optional<std::string> login = authenticate(request);
    if (!login.has_value()) {
        response.status = 401;
        return;
    }

    nlohmann::json communities = nlohmann::json::array();
    for (const Community& community : chatService_.listCommunitiesForMember(*login)) {
        communities.push_back(toJson(community, /*includeInviteCode=*/true));
    }
    response.set_content(communities.dump(), kJsonContentType);
}

void HttpServer::handleJoinCommunityByCode(const httplib::Request& request, httplib::Response& response) {
    const std::optional<std::string> login = authenticate(request);
    if (!login.has_value()) {
        response.status = 401;
        return;
    }

    if (json_guard::exceedsMaxNestingDepth(request.body, json_guard::kMaxNestingDepth)) {
        response.status = 400;
        response.set_content(nlohmann::json{{"error", "payload too deeply nested"}}.dump(), kJsonContentType);
        return;
    }
    const nlohmann::json body = nlohmann::json::parse(request.body, nullptr, /*allow_exceptions=*/false);
    if (body.is_discarded() || !body.contains("code") || !body["code"].is_string()) {
        response.status = 400;
        response.set_content(nlohmann::json{{"error", "expected 'code' string"}}.dump(), kJsonContentType);
        return;
    }

    const std::optional<Community> community = chatService_.findCommunityByInviteCode(body["code"].get<std::string>());
    if (!community.has_value()) {
        response.status = 404;
        response.set_content(nlohmann::json{{"error", "invalid invite code"}}.dump(), kJsonContentType);
        return;
    }
    // findCommunityByInviteCode() уже подтвердил, что сообщество
    // существует, так что false здесь означало бы только состояние
    // гонки — сообщество удалили в промежутке между этими двумя
    // вызовами — тот же 404, что и на невалидный код изначально, а не
    // отдельный класс ошибки, который стоило бы различать для клиента.
    if (!chatService_.joinCommunity(community->id, *login)) {
        response.status = 404;
        response.set_content(nlohmann::json{{"error", "invalid invite code"}}.dump(), kJsonContentType);
        return;
    }
    response.set_content(toJson(*community, /*includeInviteCode=*/false).dump(), kJsonContentType);
}

void HttpServer::handleRegenerateInviteCode(const httplib::Request& request, httplib::Response& response) {
    const std::optional<std::string> login = authenticate(request);
    if (!login.has_value()) {
        response.status = 401;
        return;
    }

    const auto communityId = std::stoll(request.matches[1].str());
    const RegenerateInviteCodeResult result = chatService_.regenerateInviteCode(communityId, *login);
    if (result.result != MutationResult::kSuccess) {
        writeMutationResult(result.result, response);
        return;
    }
    response.set_content(nlohmann::json{{"invite_code", result.inviteCode}}.dump(), kJsonContentType);
}

void HttpServer::writeMutationResult(MutationResult result, httplib::Response& response) {
    switch (result) {
        case MutationResult::kSuccess:
            response.status = 200;
            response.set_content(nlohmann::json{{"ok", true}}.dump(), kJsonContentType);
            return;
        case MutationResult::kNotFound:
            response.status = 404;
            response.set_content(nlohmann::json{{"error", "no such community or channel"}}.dump(), kJsonContentType);
            return;
        case MutationResult::kForbidden:
            response.status = 403;
            response.set_content(nlohmann::json{{"error", "only the owner can do that"}}.dump(), kJsonContentType);
            return;
        case MutationResult::kConflict:
            response.status = 409;
            response.set_content(nlohmann::json{{"error", "name already taken"}}.dump(), kJsonContentType);
            return;
    }
}

void HttpServer::handleRenameCommunity(const httplib::Request& request, httplib::Response& response) {
    const std::optional<std::string> login = authenticate(request);
    if (!login.has_value()) {
        response.status = 401;
        return;
    }

    if (json_guard::exceedsMaxNestingDepth(request.body, json_guard::kMaxNestingDepth)) {
        response.status = 400;
        response.set_content(nlohmann::json{{"error", "payload too deeply nested"}}.dump(), kJsonContentType);
        return;
    }
    const nlohmann::json body = nlohmann::json::parse(request.body, nullptr, /*allow_exceptions=*/false);
    if (body.is_discarded() || !body.contains("name") || !body["name"].is_string()) {
        response.status = 400;
        response.set_content(nlohmann::json{{"error", "expected 'name' string"}}.dump(), kJsonContentType);
        return;
    }

    const auto communityId = std::stoll(request.matches[1].str());
    writeMutationResult(chatService_.renameCommunity(communityId, body["name"].get<std::string>(), *login), response);
}

void HttpServer::handleDeleteCommunity(const httplib::Request& request, httplib::Response& response) {
    const std::optional<std::string> login = authenticate(request);
    if (!login.has_value()) {
        response.status = 401;
        return;
    }

    const auto communityId = std::stoll(request.matches[1].str());
    writeMutationResult(chatService_.deleteCommunity(communityId, *login), response);
}

void HttpServer::handleJoinCommunity(const httplib::Request& request, httplib::Response& response) {
    const std::optional<std::string> login = authenticate(request);
    if (!login.has_value()) {
        response.status = 401;
        return;
    }

    const auto communityId = std::stoll(request.matches[1].str());
    if (!chatService_.joinCommunity(communityId, *login)) {
        response.status = 404;
        response.set_content(nlohmann::json{{"error", "no such community"}}.dump(), kJsonContentType);
        return;
    }
    response.set_content(nlohmann::json{{"joined", true}}.dump(), kJsonContentType);
}

void HttpServer::handleCreateChannel(const httplib::Request& request, httplib::Response& response) {
    const std::optional<std::string> login = authenticate(request);
    if (!login.has_value()) {
        response.status = 401;
        return;
    }

    if (json_guard::exceedsMaxNestingDepth(request.body, json_guard::kMaxNestingDepth)) {
        response.status = 400;
        response.set_content(nlohmann::json{{"error", "payload too deeply nested"}}.dump(), kJsonContentType);
        return;
    }
    const nlohmann::json body = nlohmann::json::parse(request.body, nullptr, /*allow_exceptions=*/false);
    if (body.is_discarded() || !body.contains("name") || !body["name"].is_string()) {
        response.status = 400;
        response.set_content(nlohmann::json{{"error", "expected 'name' string"}}.dump(), kJsonContentType);
        return;
    }

    const bool isEncrypted = body.contains("is_encrypted") && body["is_encrypted"].is_boolean() &&
                              body["is_encrypted"].get<bool>();

    const auto communityId = std::stoll(request.matches[1].str());
    // Issue #256 (pentest): раньше кто угодно мог создать канал в чужом
    // сообществе, зная только его id — тот же 404, что и для реально
    // несуществующего сообщества, не подтверждаем его существование не-участнику.
    if (!chatService_.isMember(communityId, *login)) {
        response.status = 404;
        response.set_content(nlohmann::json{{"error", "no such community, or channel name already taken"}}.dump(),
                              kJsonContentType);
        return;
    }

    const std::optional<std::int64_t> channelId =
        chatService_.createChannel(communityId, body["name"].get<std::string>(), *login, isEncrypted);
    if (!channelId.has_value()) {
        response.status = 404;
        response.set_content(nlohmann::json{{"error", "no such community, or channel name already taken"}}.dump(),
                              kJsonContentType);
        return;
    }

    response.status = 201;
    response.set_content(nlohmann::json{{"id", *channelId}, {"is_encrypted", isEncrypted}}.dump(), kJsonContentType);
}

void HttpServer::handleListChannels(const httplib::Request& request, httplib::Response& response) {
    const std::optional<std::string> login = authenticate(request);
    if (!login.has_value()) {
        response.status = 401;
        return;
    }

    const auto communityId = std::stoll(request.matches[1].str());
    // Issue #256 (pentest): раньше это отдавалось любому аутентифицированному
    // пользователю, а не только участникам сообщества — 404 не подтверждает
    // существование сообщества чужаку, тот же принцип, что уже применяется
    // к личным диалогам (isThreadParticipant()).
    if (!chatService_.isMember(communityId, *login)) {
        response.status = 404;
        response.set_content(nlohmann::json{{"error", "no such community"}}.dump(), kJsonContentType);
        return;
    }

    nlohmann::json channels = nlohmann::json::array();
    for (const Channel& channel : chatService_.listChannels(communityId)) {
        channels.push_back(toJson(channel));
    }
    response.set_content(channels.dump(), kJsonContentType);
}

void HttpServer::handleRenameChannel(const httplib::Request& request, httplib::Response& response) {
    const std::optional<std::string> login = authenticate(request);
    if (!login.has_value()) {
        response.status = 401;
        return;
    }

    if (json_guard::exceedsMaxNestingDepth(request.body, json_guard::kMaxNestingDepth)) {
        response.status = 400;
        response.set_content(nlohmann::json{{"error", "payload too deeply nested"}}.dump(), kJsonContentType);
        return;
    }
    const nlohmann::json body = nlohmann::json::parse(request.body, nullptr, /*allow_exceptions=*/false);
    if (body.is_discarded() || !body.contains("name") || !body["name"].is_string()) {
        response.status = 400;
        response.set_content(nlohmann::json{{"error", "expected 'name' string"}}.dump(), kJsonContentType);
        return;
    }

    const auto channelId = std::stoll(request.matches[1].str());
    writeMutationResult(chatService_.renameChannel(channelId, body["name"].get<std::string>(), *login), response);
}

void HttpServer::handleDeleteChannel(const httplib::Request& request, httplib::Response& response) {
    const std::optional<std::string> login = authenticate(request);
    if (!login.has_value()) {
        response.status = 401;
        return;
    }

    const auto channelId = std::stoll(request.matches[1].str());
    writeMutationResult(chatService_.deleteChannel(channelId, *login), response);
}

void HttpServer::handleListMessages(const httplib::Request& request, httplib::Response& response) {
    const std::optional<std::string> login = authenticate(request);
    if (!login.has_value()) {
        response.status = 401;
        return;
    }

    const auto channelId = std::stoll(request.matches[1].str());
    // Issue #256 (pentest): раньше отдавало полную историю сообщений
    // канала любому аутентифицированному пользователю — 404 и для
    // несуществующего канала, и для существующего, но чужого (не
    // подтверждаем существование не-участнику).
    const std::optional<Channel> channel = chatService_.findChannel(channelId);
    if (!channel.has_value() || !chatService_.isMember(channel->communityId, *login)) {
        response.status = 404;
        response.set_content(nlohmann::json{{"error", "no such channel"}}.dump(), kJsonContentType);
        return;
    }

    const int limit = request.has_param("limit") ? std::stoi(request.get_param_value("limit")) : kDefaultMessageLimit;
    const std::optional<std::int64_t> beforeId =
        request.has_param("before_id") ? std::make_optional(std::stoll(request.get_param_value("before_id")))
                                        : std::nullopt;

    nlohmann::json messages = nlohmann::json::array();
    for (const Message& message : chatService_.recentMessages(channelId, limit, beforeId)) {
        messages.push_back(toJson(message));
    }
    response.set_content(messages.dump(), kJsonContentType);
}

void HttpServer::handlePromoteModerator(const httplib::Request& request, httplib::Response& response) {
    const std::optional<std::string> login = authenticate(request);
    if (!login.has_value()) {
        response.status = 401;
        return;
    }

    if (json_guard::exceedsMaxNestingDepth(request.body, json_guard::kMaxNestingDepth)) {
        response.status = 400;
        response.set_content(nlohmann::json{{"error", "payload too deeply nested"}}.dump(), kJsonContentType);
        return;
    }
    const nlohmann::json body = nlohmann::json::parse(request.body, nullptr, /*allow_exceptions=*/false);
    if (body.is_discarded() || !body.contains("login") || !body["login"].is_string()) {
        response.status = 400;
        response.set_content(nlohmann::json{{"error", "expected 'login' string"}}.dump(), kJsonContentType);
        return;
    }

    const auto communityId = std::stoll(request.matches[1].str());
    writeMutationResult(chatService_.promoteModerator(communityId, body["login"].get<std::string>(), *login),
                         response);
}

void HttpServer::handleDemoteModerator(const httplib::Request& request, httplib::Response& response) {
    const std::optional<std::string> login = authenticate(request);
    if (!login.has_value()) {
        response.status = 401;
        return;
    }

    const auto communityId = std::stoll(request.matches[1].str());
    writeMutationResult(chatService_.demoteModerator(communityId, request.matches[2].str(), *login), response);
}

void HttpServer::handleListModerators(const httplib::Request& request, httplib::Response& response) {
    const std::optional<std::string> login = authenticate(request);
    if (!login.has_value()) {
        response.status = 401;
        return;
    }

    const auto communityId = std::stoll(request.matches[1].str());
    // Issue #256 (pentest) — см. тот же комментарий в handleListChannels().
    if (!chatService_.isMember(communityId, *login)) {
        response.status = 404;
        response.set_content(nlohmann::json{{"error", "no such community"}}.dump(), kJsonContentType);
        return;
    }

    nlohmann::json moderators = nlohmann::json::array();
    for (const std::string& moderatorLogin : chatService_.listModerators(communityId)) {
        moderators.push_back(moderatorLogin);
    }
    response.set_content(moderators.dump(), kJsonContentType);
}

void HttpServer::handleUploadAttachment(const httplib::Request& request, httplib::Response& response) {
    const std::optional<std::string> login = authenticate(request);
    if (!login.has_value()) {
        response.status = 401;
        return;
    }

    if (json_guard::exceedsMaxNestingDepth(request.body, json_guard::kMaxNestingDepth)) {
        response.status = 400;
        response.set_content(nlohmann::json{{"error", "payload too deeply nested"}}.dump(), kJsonContentType);
        return;
    }
    const nlohmann::json body = nlohmann::json::parse(request.body, nullptr, /*allow_exceptions=*/false);
    if (body.is_discarded() || !body.contains("filename") || !body["filename"].is_string() ||
        !body.contains("content_type") || !body["content_type"].is_string() || !body.contains("data_base64") ||
        !body["data_base64"].is_string()) {
        response.status = 400;
        response.set_content(
            nlohmann::json{{"error", "expected 'filename', 'content_type', 'data_base64' strings"}}.dump(),
            kJsonContentType);
        return;
    }
    // Issue #226 (pentest): не было предела длины вообще — filename
    // дословно попадает в заголовок Content-Disposition при каждом
    // скачивании (см. sanitizeForHeaderValue() выше), так что
    // произвольно длинное имя было бы вектором для непропорционально
    // большого заголовка ответа на каждой попытке скачать файл снова.
    // 255 — обычный предел имени файла большинства файловых систем, не
    // специфичное для этого проекта число.
    if (body["filename"].get<std::string>().size() > kMaxAttachmentFilenameLength) {
        response.status = 400;
        response.set_content(
            nlohmann::json{{"error", "'filename' exceeds the 255-character limit"}}.dump(), kJsonContentType);
        return;
    }

    const std::string dataBase64 = body["data_base64"].get<std::string>();
    const std::optional<std::string> decoded = base64::decode(dataBase64);
    if (!decoded.has_value()) {
        response.status = 400;
        response.set_content(nlohmann::json{{"error", "'data_base64' is not valid base64"}}.dump(), kJsonContentType);
        return;
    }
    if (decoded->size() > kMaxAttachmentSizeBytes) {
        response.status = 400;
        response.set_content(nlohmann::json{{"error", "attachment exceeds the 5 MB size limit"}}.dump(),
                              kJsonContentType);
        return;
    }

    const auto channelId = std::stoll(request.matches[1].str());
    const std::optional<Channel> channel = chatService_.findChannel(channelId);
    // Issue #256 (pentest): раньше кто угодно мог загрузить файл в чужой
    // канал, зная только его id — та же ошибка, что и для реально
    // несуществующего канала, не подтверждаем его существование не-участнику.
    if (!channel.has_value() || !chatService_.isMember(channel->communityId, *login)) {
        response.status = 404;
        response.set_content(nlohmann::json{{"error", "no such channel"}}.dump(), kJsonContentType);
        return;
    }
    if (channel->isEncrypted) {
        // Вложения ещё не шифруются на стороне клиента на этом этапе
        // (issue #138) — отклоняем вместо того, чтобы молча хранить
        // открытый текст в канале, который UI представляет как зашифрованный.
        response.status = 400;
        response.set_content(
            nlohmann::json{{"error", "attachments aren't supported in encrypted channels yet"}}.dump(),
            kJsonContentType);
        return;
    }

    const std::optional<AttachmentMetadata> attachment = chatService_.createAttachment(
        channelId, AttachmentUpload{.uploaderLogin = *login,
                                     .filename = body["filename"].get<std::string>(),
                                     .contentType = body["content_type"].get<std::string>(),
                                     .dataBase64 = dataBase64,
                                     .sizeBytes = static_cast<std::int64_t>(decoded->size())});
    if (!attachment.has_value()) {
        response.status = 404;
        response.set_content(nlohmann::json{{"error", "no such channel"}}.dump(), kJsonContentType);
        return;
    }

    response.status = 201;
    response.set_content(nlohmann::json{{"id", attachment->id},
                                         {"filename", attachment->filename},
                                         {"content_type", attachment->contentType},
                                         {"size_bytes", attachment->sizeBytes}}
                              .dump(),
                          kJsonContentType);
}

void HttpServer::handleDownloadAttachment(const httplib::Request& request, httplib::Response& response) {
    const std::optional<std::string> login = authenticate(request);
    if (!login.has_value()) {
        response.status = 401;
        return;
    }

    const auto attachmentId = std::stoll(request.matches[1].str());
    const std::optional<AttachmentData> attachment = chatService_.findAttachmentData(attachmentId);
    if (!attachment.has_value()) {
        response.status = 404;
        response.set_content(nlohmann::json{{"error", "no such attachment"}}.dump(), kJsonContentType);
        return;
    }
    // Issue #256 (pentest): раньше кто угодно мог скачать любой файл,
    // зная только его id — тот же 404, что и для реально
    // несуществующего вложения, не подтверждаем его существование
    // не-участнику канала, которому оно принадлежит.
    const std::optional<Channel> channel = chatService_.findChannel(attachment->channelId);
    if (!channel.has_value() || !chatService_.isMember(channel->communityId, *login)) {
        response.status = 404;
        response.set_content(nlohmann::json{{"error", "no such attachment"}}.dump(), kJsonContentType);
        return;
    }

    // Декодируется здесь, а не хранится в сыром виде (см. doc-комментарий
    // ChatRepository) — response.set_content() бинарно-безопасен (явно
    // отслеживает длину, а не полагается на null-терминатор), поэтому
    // декодированные байты доходят до клиента ровно в том виде, в каком
    // были загружены, независимо от содержимого.
    const std::optional<std::string> decoded = base64::decode(attachment->data);
    if (!decoded.has_value()) {
        response.status = 500;
        response.set_content(nlohmann::json{{"error", "stored attachment data is corrupt"}}.dump(), kJsonContentType);
        return;
    }
    response.set_header("Content-Disposition",
                         "attachment; filename=\"" + sanitizeForHeaderValue(attachment->filename) + "\"");
    response.set_content(*decoded, attachment->contentType);
}

void HttpServer::handleSearchMessages(const httplib::Request& request, httplib::Response& response) {
    const std::optional<std::string> login = authenticate(request);
    if (!login.has_value()) {
        response.status = 401;
        return;
    }

    if (!request.has_param("q") || request.get_param_value("q").empty()) {
        response.status = 400;
        response.set_content(nlohmann::json{{"error", "expected non-empty 'q' query parameter"}}.dump(),
                              kJsonContentType);
        return;
    }

    const auto channelId = std::stoll(request.matches[1].str());
    const std::optional<Channel> channel = chatService_.findChannel(channelId);
    // Issue #256 (pentest): раньше отдавало результаты поиска по чужой
    // истории сообщений любому аутентифицированному пользователю — 404
    // и для несуществующего канала, и для существующего, но чужого.
    if (!channel.has_value() || !chatService_.isMember(channel->communityId, *login)) {
        response.status = 404;
        response.set_content(nlohmann::json{{"error", "no such channel"}}.dump(), kJsonContentType);
        return;
    }
    if (channel->isEncrypted) {
        // Поиску на стороне сервера нужны открытые тела для сопоставления
        // — сохранённое тело зашифрованного канала является шифротекстом,
        // так что искать здесь по существу нечего (issue #138).
        response.status = 400;
        response.set_content(nlohmann::json{{"error", "search isn't available for encrypted channels"}}.dump(),
                              kJsonContentType);
        return;
    }

    const std::string query = request.get_param_value("q");
    const int limit = request.has_param("limit") ? std::stoi(request.get_param_value("limit")) : kDefaultSearchLimit;

    nlohmann::json messages = nlohmann::json::array();
    for (const Message& message : chatService_.searchMessages(channelId, query, limit)) {
        messages.push_back(toJson(message));
    }
    response.set_content(messages.dump(), kJsonContentType);
}

void HttpServer::handleListMembers(const httplib::Request& request, httplib::Response& response) {
    const std::optional<std::string> login = authenticate(request);
    if (!login.has_value()) {
        response.status = 401;
        return;
    }

    const auto communityId = std::stoll(request.matches[1].str());
    // Issue #256 (pentest) — см. тот же комментарий в handleListChannels().
    if (!chatService_.isMember(communityId, *login)) {
        response.status = 404;
        response.set_content(nlohmann::json{{"error", "no such community"}}.dump(), kJsonContentType);
        return;
    }

    nlohmann::json members = nlohmann::json::array();
    for (const std::string& memberLogin : chatService_.listMembers(communityId)) {
        members.push_back(memberLogin);
    }
    response.set_content(members.dump(), kJsonContentType);
}

void HttpServer::handleSetChannelKey(const httplib::Request& request, httplib::Response& response) {
    const std::optional<std::string> login = authenticate(request);
    if (!login.has_value()) {
        response.status = 401;
        return;
    }

    if (json_guard::exceedsMaxNestingDepth(request.body, json_guard::kMaxNestingDepth)) {
        response.status = 400;
        response.set_content(nlohmann::json{{"error", "payload too deeply nested"}}.dump(), kJsonContentType);
        return;
    }
    const nlohmann::json body = nlohmann::json::parse(request.body, nullptr, /*allow_exceptions=*/false);
    if (body.is_discarded() || !body.contains("wrapped_key") || !body["wrapped_key"].is_string()) {
        response.status = 400;
        response.set_content(nlohmann::json{{"error", "expected 'wrapped_key' string"}}.dump(), kJsonContentType);
        return;
    }

    const auto channelId = std::stoll(request.matches[1].str());
    const std::string memberLogin = request.matches[2].str();
    writeMutationResult(
        chatService_.setChannelKey(channelId, memberLogin, *login, body["wrapped_key"].get<std::string>()),
        response);
}

void HttpServer::handleGetMyChannelKey(const httplib::Request& request, httplib::Response& response) {
    const std::optional<std::string> login = authenticate(request);
    if (!login.has_value()) {
        response.status = 401;
        return;
    }

    const auto channelId = std::stoll(request.matches[1].str());
    const std::optional<std::string> wrappedKey = chatService_.findChannelKey(channelId, *login);
    if (!wrappedKey.has_value()) {
        response.status = 404;
        response.set_content(
            nlohmann::json{{"error", "no key set for you on this channel yet"}}.dump(), kJsonContentType);
        return;
    }
    response.set_content(nlohmann::json{{"wrapped_key", *wrappedKey}}.dump(), kJsonContentType);
}

void HttpServer::handleOpenThread(const httplib::Request& request, httplib::Response& response) {
    const std::optional<std::string> login = authenticate(request);
    if (!login.has_value()) {
        response.status = 401;
        return;
    }

    const std::optional<std::string> recipientLogin = parseRecipientLogin(request.body);
    if (!recipientLogin.has_value()) {
        response.status = 400;
        response.set_content(nlohmann::json{{"error", "expected a 'recipient_login' string"}}.dump(),
                              kJsonContentType);
        return;
    }
    if (*recipientLogin == *login) {
        response.status = 400;
        response.set_content(nlohmann::json{{"error", "cannot message yourself"}}.dump(), kJsonContentType);
        return;
    }

    // Дружба принадлежит user-service, не этой базе (issue #187, Фаза
    // 2) — уже открытый диалог продолжает работать, даже если дружба
    // позже разорвётся, поэтому проверка только здесь, не в
    // handlePostDirectMessage()/handleListDirectMessages().
    if (!userServiceClient_.areFriends(*login, *recipientLogin)) {
        response.status = 403;
        response.set_content(nlohmann::json{{"error", "can only message friends"}}.dump(), kJsonContentType);
        return;
    }

    const std::int64_t threadId = chatService_.findOrCreateThread(*login, *recipientLogin);
    response.set_content(nlohmann::json{{"id", threadId}}.dump(), kJsonContentType);
}

void HttpServer::handleListMyThreads(const httplib::Request& request, httplib::Response& response) {
    const std::optional<std::string> login = authenticate(request);
    if (!login.has_value()) {
        response.status = 401;
        return;
    }

    nlohmann::json threads = nlohmann::json::array();
    for (const DirectMessageThread& thread : chatService_.listMyThreads(*login)) {
        threads.push_back(toJson(thread));
    }
    response.set_content(threads.dump(), kJsonContentType);
}

void HttpServer::handlePostDirectMessage(const httplib::Request& request, httplib::Response& response) {
    const std::optional<std::string> login = authenticate(request);
    if (!login.has_value()) {
        response.status = 401;
        return;
    }

    const auto threadId = std::stoll(request.matches[1].str());
    // 404, а не 403, для не-участника — не подтверждает существование
    // чужого диалога (тот же приём приватности, что и у findChannelKey()
    // выше, просто на уровень строже, поскольку личный диалог — не
    // полу-публичный канал сообщества).
    if (!chatService_.isThreadParticipant(threadId, *login)) {
        response.status = 404;
        response.set_content(nlohmann::json{{"error", "no such thread"}}.dump(), kJsonContentType);
        return;
    }

    const std::optional<std::string> body = parseMessageBody(request.body);
    if (!body.has_value()) {
        response.status = 400;
        response.set_content(nlohmann::json{{"error", "expected a 'body' string"}}.dump(), kJsonContentType);
        return;
    }

    const std::optional<DirectMessage> message = chatService_.postDirectMessage(threadId, *login, *body);
    if (!message.has_value()) {
        // Не должно происходить — isThreadParticipant() выше уже
        // подтвердил, что threadId существует, а удалить диалог через
        // этот API нельзя (ни один эндпоинт этого не делает) — но
        // optional сигнализирует о возможности сбоя, и разыменовывать
        // его без проверки было бы неопределённым поведением, если это
        // когда-нибудь перестанет быть верным.
        response.status = 404;
        response.set_content(nlohmann::json{{"error", "no such thread"}}.dump(), kJsonContentType);
        return;
    }
    response.status = 201;
    response.set_content(toJson(*message).dump(), kJsonContentType);
}

void HttpServer::handleListDirectMessages(const httplib::Request& request, httplib::Response& response) {
    const std::optional<std::string> login = authenticate(request);
    if (!login.has_value()) {
        response.status = 401;
        return;
    }

    const auto threadId = std::stoll(request.matches[1].str());
    if (!chatService_.isThreadParticipant(threadId, *login)) {
        response.status = 404;
        response.set_content(nlohmann::json{{"error", "no such thread"}}.dump(), kJsonContentType);
        return;
    }

    const int limit = request.has_param("limit") ? std::stoi(request.get_param_value("limit")) : kDefaultMessageLimit;
    const std::optional<std::int64_t> beforeId =
        request.has_param("before_id") ? std::make_optional(std::stoll(request.get_param_value("before_id")))
                                        : std::nullopt;

    nlohmann::json messages = nlohmann::json::array();
    for (const DirectMessage& message : chatService_.listDirectMessages(threadId, limit, beforeId)) {
        messages.push_back(toJson(message));
    }
    response.set_content(messages.dump(), kJsonContentType);
}

void HttpServer::listen(const std::string& host, int port) {
    server_.listen(host, port);
}

void HttpServer::stop() {
    server_.stop();
}

}  // namespace chat_service
