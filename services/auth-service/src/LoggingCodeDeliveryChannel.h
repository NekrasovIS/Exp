#pragma once

#include "CodeDeliveryChannel.h"

namespace auth_service {

/**
 * @brief Запасной канал, когда реальный не настроен (например, не
 *        заданы переменные окружения SMTP_*) — просто логирует код в
 *        stdout вместо реальной отправки (issue #156). Позволяет
 *        локальной разработке и CI прогонять весь flow входа по коду
 *        целиком без настоящих учётных данных email/SMS/Telegram.
 */
class LoggingCodeDeliveryChannel : public ICodeDeliveryChannel {
public:
    void send(const std::string& destination, const std::string& code) const override;
};

}  // namespace auth_service
