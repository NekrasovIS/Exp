#pragma once

#include <httplib.h>

#include <string>

#include "UserService.h"

namespace user_service {

/**
 * @brief REST front-end for UserService: POST /users/register,
 *        POST /users/verify-credentials.
 *
 * Thin wrapper around httplib::Server — all account logic lives in
 * UserService, this class only translates HTTP requests/responses.
 */
class HttpServer {
public:
    explicit HttpServer(UserService& userService);

    /// Blocks, serving requests until stop() is called from another thread.
    void listen(const std::string& host, int port);

    /// Stops a listen() call in progress.
    void stop();

private:
    void registerRoutes();
    void handleRegister(const httplib::Request& request, httplib::Response& response);
    void handleVerifyCredentials(const httplib::Request& request, httplib::Response& response);

    UserService& userService_;
    httplib::Server server_;
};

}  // namespace user_service
