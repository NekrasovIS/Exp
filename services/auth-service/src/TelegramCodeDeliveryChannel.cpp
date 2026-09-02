#include "TelegramCodeDeliveryChannel.h"

#include "TlsConnection.h"

#include <nlohmann/json.hpp>

#include <cstdlib>
#include <iostream>
#include <sstream>
#include <utility>

namespace auth_service {

namespace {

constexpr const char* kApiHost = "api.telegram.org";
constexpr int kApiPort = 443;

std::string buildMessageText(const std::string& code) {
    return "Your one-time DeviceHub sign-in code is: " + code +
           "\nIt expires in a few minutes. If you didn't request this, you can ignore this message.";
}

/// HTTP/1.1 status line looks like "HTTP/1.1 200 OK" — the status code
/// is the second whitespace-separated token.
int parseHttpStatusCode(const std::string& statusLine) {
    const std::size_t firstSpace = statusLine.find(' ');
    if (firstSpace == std::string::npos) {
        return 0;
    }
    return std::atoi(statusLine.c_str() + firstSpace + 1);
}

std::string envOrEmpty(const char* name) {
    const char* value = std::getenv(name);
    return value != nullptr ? std::string(value) : std::string();
}

}  // namespace

TelegramCodeDeliveryChannel::TelegramCodeDeliveryChannel(TelegramConfig config) : config_(std::move(config)) {}

void TelegramCodeDeliveryChannel::send(const std::string& destination, const std::string& code) const {
    // Тот же принцип "fail closed, не роняем обработчик запроса", что
    // и у SmtpCodeDeliveryChannel — /auth/otp/request всё равно
    // отвечает 200 независимо от результата доставки.
    const auto fail = [&destination](const char* step) {
        std::cerr << "[OTP] Telegram-шаг \"" << step << "\" завершился ошибкой при отправке кода в чат "
                   << destination << std::endl;
    };

    TlsConnection connection(kApiHost, kApiPort);
    if (!connection.isConnected()) {
        fail("TLS-соединение");
        return;
    }

    const std::string body =
        nlohmann::json{{"chat_id", destination}, {"text", buildMessageText(code)}}.dump();

    std::ostringstream request;
    request << "POST /bot" << config_.botToken << "/sendMessage HTTP/1.1\r\n"
            << "Host: " << kApiHost << "\r\n"
            << "Content-Type: application/json\r\n"
            << "Content-Length: " << body.size() << "\r\n"
            << "Connection: close\r\n"
            << "\r\n"
            << body;
    connection.writeRaw(request.str());

    const std::string statusLine = connection.readLine();
    if (parseHttpStatusCode(statusLine) != 200) {
        fail("HTTP-ответ");
    }
}

std::optional<TelegramConfig> TelegramCodeDeliveryChannel::fromEnvironment() {
    const std::string botToken = envOrEmpty("TELEGRAM_BOT_TOKEN");
    if (botToken.empty()) {
        return std::nullopt;
    }
    return TelegramConfig{.botToken = botToken};
}

}  // namespace auth_service
