#pragma once

#include "CodeDeliveryChannel.h"

#include <optional>
#include <string>

namespace auth_service {

/// Параметры доступа к Telegram Bot API — один токен, но в структуре
/// (а не голым std::string-полем класса), чтобы конструктор канала
/// оставался по той же форме, что и SmtpConfig/SmtpCodeDeliveryChannel.
struct TelegramConfig {
    std::string botToken;
};

/**
 * @brief Отправка кода в Telegram через Bot API (issue #174) — как и
 *        SmtpCodeDeliveryChannel, напрямую поверх OpenSSL (TlsConnection),
 *        без libcurl: Bot API — это один HTTPS POST с JSON-телом,
 *        не требующий отдельной библиотеки для HTTP-клиента.
 *
 * @p destination в send() — chat_id получателя (числовой идентификатор
 * Telegram-чата пользователя с ботом, привязанный в профиле —
 * см. UserProfile::telegramChatId), не username и не номер телефона.
 *
 * Ошибки отправки не бросают исключение — только логируются в stderr,
 * тот же принцип "fail closed", что и у SmtpCodeDeliveryChannel.
 */
class TelegramCodeDeliveryChannel : public ICodeDeliveryChannel {
public:
    explicit TelegramCodeDeliveryChannel(TelegramConfig config);

    void send(const std::string& destination, const std::string& code) const override;

    /// Читает TELEGRAM_BOT_TOKEN из переменных окружения. @return
    /// std::nullopt, если не задан — тогда HttpServer не создаёт этот
    /// канал вообще и доставка идёт по email/логированием, как и без
    /// Telegram-поддержки.
    [[nodiscard]] static std::optional<TelegramConfig> fromEnvironment();

private:
    TelegramConfig config_;
};

}  // namespace auth_service
