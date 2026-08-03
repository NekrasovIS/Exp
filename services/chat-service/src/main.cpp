#include <cstdlib>
#include <iostream>
#include <string>

#include "AuthServiceClient.h"
#include "ChatRepository.h"
#include "ChatService.h"
#include "HttpServer.h"
#include "WebSocketServer.h"

namespace {

std::string envOrDefault(const char* name, const std::string& defaultValue) {
    const char* value = std::getenv(name);
    return value != nullptr ? std::string(value) : defaultValue;
}

}  // namespace

int main() {
    // Matches docker-compose.yml's default local Postgres for chat-service.
    const std::string connectionString = envOrDefault(
        "CHAT_SERVICE_DATABASE_URL", "postgresql://chat_service:dev-only-password@localhost:5434/chat_service");
    const std::string host = envOrDefault("CHAT_SERVICE_HOST", "127.0.0.1");
    const int restPort = std::stoi(envOrDefault("CHAT_SERVICE_REST_PORT", "8082"));
    const int wsPort = std::stoi(envOrDefault("CHAT_SERVICE_WS_PORT", "8083"));
    const std::string authServiceHost = envOrDefault("AUTH_SERVICE_HOST", "127.0.0.1");
    const int authServicePort = std::stoi(envOrDefault("AUTH_SERVICE_PORT", "8080"));

    chat_service::ChatRepository repository(connectionString);
    chat_service::ChatService chatService(repository);
    const chat_service::AuthServiceClient authServiceClient(authServiceHost, authServicePort);

    chat_service::WebSocketServer webSocketServer(chatService, authServiceClient, wsPort, host);
    if (!webSocketServer.start()) {
        std::cerr << "chat-service: failed to start WebSocket server on " << host << ":" << wsPort << "\n";
        return 1;
    }
    std::cout << "chat-service: WebSocket listening on " << host << ":" << wsPort << "\n";

    chat_service::HttpServer httpServer(chatService, authServiceClient);
    std::cout << "chat-service: REST listening on " << host << ":" << restPort << "\n";
    httpServer.listen(host, restPort);

    webSocketServer.stop();
    return 0;
}
