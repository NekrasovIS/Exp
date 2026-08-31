#pragma once

#include <httplib.h>

#include <chrono>
#include <string>

#include "RateLimiter.h"
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
 *
 * /auth/token and /auth/register (both credential-checking) are rate
 * limited per client address (issue #102) — /auth/verify and
 * /auth/refresh aren't: /auth/verify is called legitimately by
 * chat-service on every single request it handles, and /auth/refresh
 * requires an already-valid signed refresh token rather than a
 * guessable credential, so neither should trip the limiter meant for
 * brute-force guarding.
 */
class HttpServer {
public:
    /// @p rateLimitMaxRequests/@p rateLimitWindow configure the
    /// /auth/token + /auth/register limiter — defaults are a real
    /// production limit; tests pass a tiny window to trip it fast and
    /// deterministically instead of waiting on a real clock.
    HttpServer(const TokenService& tokenService, const UserServiceClient& userServiceClient,
               int rateLimitMaxRequests = 10, std::chrono::milliseconds rateLimitWindow = std::chrono::seconds{60});

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
    RateLimiter rateLimiter_;
    httplib::Server server_;
};

}  // namespace auth_service
