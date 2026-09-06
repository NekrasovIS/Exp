#pragma once

#include <string>

namespace auth_service {

/**
 * @brief Абстракция над способом доставки одноразового кода (issue
 *        #156) — сегодня email, SMS/Telegram естественно ложатся как
 *        дополнительные реализации того же интерфейса.
 */
class ICodeDeliveryChannel {
public:
    virtual ~ICodeDeliveryChannel() = default;

    /// Отправляет @p code на @p destination (email/номер телефона/
    /// идентификатор чата — зависит от реализации).
    virtual void send(const std::string& destination, const std::string& code) const = 0;
};

}  // namespace auth_service
