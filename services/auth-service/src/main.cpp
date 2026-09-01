#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>

#include "CodeDeliveryChannel.h"
#include "HttpServer.h"
#include "LoggingCodeDeliveryChannel.h"
#include "SmtpCodeDeliveryChannel.h"
#include "TelegramCodeDeliveryChannel.h"
#include "TokenService.h"
#include "UserServiceClient.h"

namespace {

std::string envOrDefault(const char* name, const std::string& defaultValue) {
    const char* value = std::getenv(name);
    return value != nullptr ? std::string(value) : defaultValue;
}

}  // namespace

int main() {
    const char* secret = std::getenv("AUTH_SERVICE_SECRET");
    if (secret == nullptr || std::string(secret).empty()) {
        std::cerr << "AUTH_SERVICE_SECRET environment variable is required (and must be non-empty) — "
                      "refusing to start with no signing key.\n";
        return 1;
    }

    const std::string host = envOrDefault("AUTH_SERVICE_HOST", "127.0.0.1");
    const int port = std::stoi(envOrDefault("AUTH_SERVICE_PORT", "8080"));
    const std::string userServiceHost = envOrDefault("USER_SERVICE_HOST", "127.0.0.1");
    const int userServicePort = std::stoi(envOrDefault("USER_SERVICE_PORT", "8081"));

    const auth_service::TokenService tokenService(secret);
    const auth_service::UserServiceClient userServiceClient(userServiceHost, userServicePort);

    // SMTP настроен только если задан SMTP_HOST — иначе коды входа
    // просто логируются (issue #156), чтобы dev/CI не требовали
    // реальных учётных данных email для сквозной проверки flow.
    std::unique_ptr<auth_service::ICodeDeliveryChannel> codeDeliveryChannel;
    if (const auto smtpConfig = auth_service::SmtpCodeDeliveryChannel::fromEnvironment(); smtpConfig.has_value()) {
        codeDeliveryChannel = std::make_unique<auth_service::SmtpCodeDeliveryChannel>(*smtpConfig);
        std::cout << "One-time-code delivery: SMTP (" << smtpConfig->host << ")\n";
    } else {
        codeDeliveryChannel = std::make_unique<auth_service::LoggingCodeDeliveryChannel>();
        std::cout << "One-time-code delivery: logging only (SMTP_HOST not set)\n";
    }

    // Telegram настроен только если задан TELEGRAM_BOT_TOKEN (issue
    // #174) — иначе доставка идёт по email/логированием, как и раньше;
    // канал остаётся необязательным (nullptr), не заменяет
    // codeDeliveryChannel, а лишь имеет приоритет над ним на аккаунт.
    std::unique_ptr<auth_service::ICodeDeliveryChannel> telegramChannel;
    if (const auto telegramConfig = auth_service::TelegramCodeDeliveryChannel::fromEnvironment();
        telegramConfig.has_value()) {
        telegramChannel = std::make_unique<auth_service::TelegramCodeDeliveryChannel>(*telegramConfig);
        std::cout << "One-time-code delivery: Telegram bot configured (preferred over email when both are set)\n";
    }

    auth_service::HttpServer server(tokenService, userServiceClient, *codeDeliveryChannel, telegramChannel.get());

    std::cout << "auth-service listening on " << host << ":" << port << "\n";
    server.listen(host, port);

    return 0;
}
