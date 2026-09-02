#pragma once

#include "CodeDeliveryChannel.h"

#include <optional>
#include <string>

namespace auth_service {

/// Параметры подключения к SMTP-серверу — сгруппированы в структуру
/// вместо длинного списка параметров конструктора (правило проекта про
/// количество аргументов).
struct SmtpConfig {
    std::string host;
    int port = 465;
    std::string username;
    std::string password;
    std::string fromAddress;
};

/**
 * @brief Отправка кода по email через SMTPS напрямую поверх OpenSSL
 *        (issue #156) — без libcurl: OpenSSL уже зависимость проекта
 *        (TokenService), а протокол SMTP (RFC 5321) поверх готового TLS-
 *        соединения — это всего несколько текстовых команд/ответов, не
 *        требующих отдельной библиотеки.
 *
 * Ошибки отправки (сеть недоступна, неверные учётные данные, сервер
 * отклонил письмо и т.п.) не бросают исключение — только логируются в
 * stderr, тем же принципом «fail closed, не роняем обработчик запроса»,
 * что уже применяется в UserServiceClient.
 */
class SmtpCodeDeliveryChannel : public ICodeDeliveryChannel {
public:
    explicit SmtpCodeDeliveryChannel(SmtpConfig config);

    void send(const std::string& destination, const std::string& code) const override;

    /// Читает SMTP_HOST/SMTP_PORT/SMTP_USERNAME/SMTP_PASSWORD/SMTP_FROM
    /// из переменных окружения. @return std::nullopt, если SMTP_HOST не
    /// задан — минимум, по которому решаем, что SMTP вообще настроен
    /// (тогда HttpServer использует LoggingCodeDeliveryChannel вместо
    /// этого канала).
    [[nodiscard]] static std::optional<SmtpConfig> fromEnvironment();

private:
    SmtpConfig config_;
};

}  // namespace auth_service
