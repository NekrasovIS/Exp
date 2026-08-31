#pragma once

#include <httplib.h>

#include <optional>
#include <string>

#include "AuthServiceClient.h"
#include "UserService.h"

namespace user_service {

/**
 * @brief REST front-end for UserService: POST /users/register,
 *        POST /users/verify-credentials (both unauthenticated — called by
 *        auth-service itself, not directly by end-user clients), plus
 *        GET /users/{login}/profile and PATCH /users/me (issue #110),
 *        which require a valid `Authorization: Bearer <token>` header,
 *        checked against auth-service via AuthServiceClient.
 *
 * PATCH /users/me always writes to the token's own subject — the login
 * in the URL/body, if any, is ignored, so a caller can never edit
 * another user's profile.
 *
 * Thin wrapper around httplib::Server — all account logic lives in
 * UserService, this class only translates HTTP requests/responses.
 */
class HttpServer {
public:
    HttpServer(UserService& userService, const AuthServiceClient& authServiceClient);

    /// Blocks, serving requests until stop() is called from another thread.
    void listen(const std::string& host, int port);

    /// Stops a listen() call in progress.
    void stop();

private:
    void registerRoutes();
    [[nodiscard]] std::optional<std::string> authenticate(const httplib::Request& request) const;

    void handleRegister(const httplib::Request& request, httplib::Response& response);
    void handleVerifyCredentials(const httplib::Request& request, httplib::Response& response);
    void handleGetProfile(const httplib::Request& request, httplib::Response& response);
    void handleUpdateOwnProfile(const httplib::Request& request, httplib::Response& response);

    UserService& userService_;
    const AuthServiceClient& authServiceClient_;
    httplib::Server server_;
};

}  // namespace user_service
