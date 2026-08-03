#pragma once

#include <httplib.h>

#include <string>

#include "TokenService.h"
#include "UserServiceClient.h"

namespace auth_service {

/**
 * @brief REST front-end for TokenService: POST /auth/token, POST /auth/verify.
 *
 * POST /auth/token now requires valid {"login", "password"} — checked
 * against user-service via UserServiceClient — before a token is issued.
 * Thin wrapper around httplib::Server otherwise — all token logic lives
 * in TokenService, this class only translates HTTP requests/responses.
 */
class HttpServer {
public:
    HttpServer(const TokenService& tokenService, const UserServiceClient& userServiceClient);

    /// Blocks, serving requests until stop() is called from another thread.
    void listen(const std::string& host, int port);

    /// Stops a listen() call in progress.
    void stop();

private:
    void registerRoutes();
    void handleIssueToken(const httplib::Request& request, httplib::Response& response);
    void handleVerifyToken(const httplib::Request& request, httplib::Response& response);

    const TokenService& tokenService_;
    const UserServiceClient& userServiceClient_;
    httplib::Server server_;
};

}  // namespace auth_service
