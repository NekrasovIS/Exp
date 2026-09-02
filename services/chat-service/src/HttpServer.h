#pragma once

#include <httplib.h>

#include <optional>
#include <string>

#include "AuthServiceClient.h"
#include "ChatService.h"

namespace chat_service {

/**
 * @brief REST-фронтенд для ChatService: сообщества, каналы, вступление,
 *        управление модераторами (issue #114), история сообщений,
 *        загрузка/скачивание вложений (issue #116), поиск сообщений
 *        (issue #118) и обмен пер-участник ключами канала для E2E-
 *        шифрования (issue #138). Каждый маршрут требует действительный
 *        заголовок `Authorization: Bearer <token>`, проверяемый через
 *        auth-service посредством AuthServiceClient.
 *
 * Назначение/снятие модератора — только для владельца: kForbidden
 * ("only the owner can do that") из writeMutationResult() уже покрывает
 * это, как и переименование/удаление сообщества.
 *
 * Доставка сообщений в реальном времени — задача WebSocketServer, а не
 * этого класса — здесь только история/CRUD.
 */
class HttpServer {
public:
    HttpServer(ChatService& chatService, const AuthServiceClient& authServiceClient);

    /// Блокируется, обслуживая запросы, пока stop() не будет вызван из другого потока.
    void listen(const std::string& host, int port);

    /// Останавливает выполняющийся вызов listen().
    void stop();

private:
    void registerRoutes();
    [[nodiscard]] std::optional<std::string> authenticate(const httplib::Request& request) const;

    void handleCreateCommunity(const httplib::Request& request, httplib::Response& response);
    void handleListCommunities(const httplib::Request& request, httplib::Response& response);
    void handleRenameCommunity(const httplib::Request& request, httplib::Response& response);
    void handleDeleteCommunity(const httplib::Request& request, httplib::Response& response);
    void handleJoinCommunity(const httplib::Request& request, httplib::Response& response);
    void handlePromoteModerator(const httplib::Request& request, httplib::Response& response);
    void handleDemoteModerator(const httplib::Request& request, httplib::Response& response);
    void handleListModerators(const httplib::Request& request, httplib::Response& response);
    void handleCreateChannel(const httplib::Request& request, httplib::Response& response);
    void handleListChannels(const httplib::Request& request, httplib::Response& response);
    void handleRenameChannel(const httplib::Request& request, httplib::Response& response);
    void handleDeleteChannel(const httplib::Request& request, httplib::Response& response);
    void handleListMessages(const httplib::Request& request, httplib::Response& response);
    void handleUploadAttachment(const httplib::Request& request, httplib::Response& response);
    void handleDownloadAttachment(const httplib::Request& request, httplib::Response& response);
    void handleSearchMessages(const httplib::Request& request, httplib::Response& response);
    void handleListMembers(const httplib::Request& request, httplib::Response& response);
    void handleSetChannelKey(const httplib::Request& request, httplib::Response& response);
    void handleGetMyChannelKey(const httplib::Request& request, httplib::Response& response);
    void writeMutationResult(MutationResult result, httplib::Response& response);

    ChatService& chatService_;
    const AuthServiceClient& authServiceClient_;
    httplib::Server server_;
};

}  // namespace chat_service
