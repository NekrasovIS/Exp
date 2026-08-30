#pragma once

#include <httplib.h>

#include <string>

#include "TokenService.h"
#include "UserServiceClient.h"

namespace auth_service {

/**
 * @brief REST front-end for TokenService: POST /auth/token,
 *        POST /auth/verify, POST /auth/register, POST /auth/refresh.
 *
 * POST /auth/token requires valid {"login", "password"} — checked
 * against user-service via UserServiceClient — before a token is issued.
 * POST /auth/register forwards to user-service's own registration and,
 * on success, immediately issues a token too (auto-login) so the client
 * doesn't need a second round trip. Both also return a long-lived
 * refresh_token (issue #105) alongside the access token; POST
 * /auth/refresh exchanges a still-valid refresh token for a fresh
 * access token via {"refresh_token"} — without the user re-entering
 * credentials — same response shape as /auth/token. Thin wrapper around
 * httplib::Server otherwise — all token logic lives in TokenService,
 * this class only translates HTTP requests/responses.
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
    void handleRegister(const httplib::Request& request, httplib::Response& response);
    void handleRefresh(const httplib::Request& request, httplib::Response& response);

    const TokenService& tokenService_;
    const UserServiceClient& userServiceClient_;
    httplib::Server server_;
};

}  // namespace auth_service
