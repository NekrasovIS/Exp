#pragma once

#include <httplib.h>

#include <string>

#include "TokenService.h"

namespace auth_service {

/**
 * @brief REST front-end for TokenService: POST /auth/token, POST /auth/verify.
 *
 * Thin wrapper around httplib::Server — all token logic lives in
 * TokenService, this class only translates HTTP requests/responses.
 */
class HttpServer {
public:
    explicit HttpServer(const TokenService& tokenService);

    /// Blocks, serving requests until stop() is called from another thread.
    void listen(const std::string& host, int port);

    /// Stops a listen() call in progress.
    void stop();

private:
    void registerRoutes();
    void handleIssueToken(const httplib::Request& request, httplib::Response& response);
    void handleVerifyToken(const httplib::Request& request, httplib::Response& response);

    const TokenService& tokenService_;
    httplib::Server server_;
};

}  // namespace auth_service
