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
 *        POST /auth/verify, POST /auth/register.
 *
 * POST /auth/token requires valid {"login", "password"} — checked
 * against user-service via UserServiceClient — before a token is issued.
 * POST /auth/register forwards to user-service's own registration and,
 * on success, immediately issues a token too (auto-login) so the client
 * doesn't need a second round trip. Thin wrapper around httplib::Server
 * otherwise — all token logic lives in TokenService, this class only
 * translates HTTP requests/responses.
 *
 * /auth/token and /auth/register (both credential-checking) are rate
 * limited per client address (issue #102) — /auth/verify isn't, since
 * chat-service calls it legitimately on every single request it
 * handles and would trip the same limiter meant for brute-force
 * guarding.
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

    const TokenService& tokenService_;
    const UserServiceClient& userServiceClient_;
    RateLimiter rateLimiter_;
    httplib::Server server_;
};

}  // namespace auth_service
