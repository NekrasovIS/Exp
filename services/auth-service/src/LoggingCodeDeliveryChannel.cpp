#include "LoggingCodeDeliveryChannel.h"

#include <iostream>

namespace auth_service {

void LoggingCodeDeliveryChannel::send(const std::string& destination, const std::string& code) const {
    // Код — не секрет уровня пароля (короткоживущий, одноразовый), но
    // в проде здесь всё равно был бы реальный канал, а не лог; в
    // dev/CI без настоящих SMTP-данных это единственный способ увидеть
    // код и пройти flow вручную.
    std::cout << "[OTP] Канал доставки не настроен — код для " << destination << ": " << code << std::endl;
}

}  // namespace auth_service
