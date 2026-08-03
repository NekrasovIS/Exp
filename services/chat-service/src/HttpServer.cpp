#include "HttpServer.h"

#include <nlohmann/json.hpp>

#include <string_view>

namespace chat_service {

namespace {
constexpr const char* kJsonContentType = "application/json";
constexpr std::string_view kBearerPrefix = "Bearer ";
constexpr int kDefaultMessageLimit = 50;

nlohmann::json toJson(const Community& community) {
    return nlohmann::json{{"id", community.id}, {"name", community.name}};
}

nlohmann::json toJson(const Channel& channel) {
    return nlohmann::json{{"id", channel.id}, {"community_id", channel.communityId}, {"name", channel.name}};
}

nlohmann::json toJson(const Message& message) {
    return nlohmann::json{
        {"id", message.id}, {"author", message.authorLogin}, {"body", message.body}, {"sent_at", message.sentAt}};
}
}  // namespace

HttpServer::HttpServer(ChatService& chatService, const AuthServiceClient& authServiceClient)
    : chatService_(chatService), authServiceClient_(authServiceClient) {
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
    server_.Post(R"(/communities/(\d+)/join)",
                  [this](const httplib::Request& request, httplib::Response& response) {
                      handleJoinCommunity(request, response);
                  });
    server_.Post(R"(/communities/(\d+)/channels)",
                  [this](const httplib::Request& request, httplib::Response& response) {
                      handleCreateChannel(request, response);
                  });
    server_.Get(R"(/communities/(\d+)/channels)",
                 [this](const httplib::Request& request, httplib::Response& response) {
                     handleListChannels(request, response);
                 });
    server_.Get(R"(/channels/(\d+)/messages)",
                 [this](const httplib::Request& request, httplib::Response& response) {
                     handleListMessages(request, response);
                 });
}

void HttpServer::handleCreateCommunity(const httplib::Request& request, httplib::Response& response) {
    if (!authenticate(request).has_value()) {
        response.status = 401;
        return;
    }

    const nlohmann::json body = nlohmann::json::parse(request.body, nullptr, /*allow_exceptions=*/false);
    if (body.is_discarded() || !body.contains("name") || !body["name"].is_string()) {
        response.status = 400;
        response.set_content(nlohmann::json{{"error", "expected 'name' string"}}.dump(), kJsonContentType);
        return;
    }

    const Community community = chatService_.createCommunity(body["name"].get<std::string>());
    response.status = 201;
    response.set_content(toJson(community).dump(), kJsonContentType);
}

void HttpServer::handleListCommunities(const httplib::Request& request, httplib::Response& response) {
    if (!authenticate(request).has_value()) {
        response.status = 401;
        return;
    }

    nlohmann::json communities = nlohmann::json::array();
    for (const Community& community : chatService_.listCommunities()) {
        communities.push_back(toJson(community));
    }
    response.set_content(communities.dump(), kJsonContentType);
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
    if (!authenticate(request).has_value()) {
        response.status = 401;
        return;
    }

    const nlohmann::json body = nlohmann::json::parse(request.body, nullptr, /*allow_exceptions=*/false);
    if (body.is_discarded() || !body.contains("name") || !body["name"].is_string()) {
        response.status = 400;
        response.set_content(nlohmann::json{{"error", "expected 'name' string"}}.dump(), kJsonContentType);
        return;
    }

    const auto communityId = std::stoll(request.matches[1].str());
    const std::optional<std::int64_t> channelId = chatService_.createChannel(communityId, body["name"].get<std::string>());
    if (!channelId.has_value()) {
        response.status = 404;
        response.set_content(nlohmann::json{{"error", "no such community, or channel name already taken"}}.dump(),
                              kJsonContentType);
        return;
    }

    response.status = 201;
    response.set_content(nlohmann::json{{"id", *channelId}}.dump(), kJsonContentType);
}

void HttpServer::handleListChannels(const httplib::Request& request, httplib::Response& response) {
    if (!authenticate(request).has_value()) {
        response.status = 401;
        return;
    }

    const auto communityId = std::stoll(request.matches[1].str());
    nlohmann::json channels = nlohmann::json::array();
    for (const Channel& channel : chatService_.listChannels(communityId)) {
        channels.push_back(toJson(channel));
    }
    response.set_content(channels.dump(), kJsonContentType);
}

void HttpServer::handleListMessages(const httplib::Request& request, httplib::Response& response) {
    if (!authenticate(request).has_value()) {
        response.status = 401;
        return;
    }

    const auto channelId = std::stoll(request.matches[1].str());
    const int limit = request.has_param("limit") ? std::stoi(request.get_param_value("limit")) : kDefaultMessageLimit;

    nlohmann::json messages = nlohmann::json::array();
    for (const Message& message : chatService_.recentMessages(channelId, limit)) {
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
