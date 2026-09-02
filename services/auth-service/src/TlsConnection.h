#pragma once

#include <string>

typedef struct bio_st BIO;
typedef struct ssl_ctx_st SSL_CTX;

namespace auth_service {

/**
 * @brief Тонкая обёртка над OpenSSL BIO для построчного TLS-соединения
 *        — общая транспортная основа для любого простого текстового
 *        протокола поверх TLS без внешней библиотеки вроде libcurl
 *        (issue #156: SMTP для SmtpCodeDeliveryChannel; issue #174:
 *        HTTP/1.1 для TelegramCodeDeliveryChannel).
 *
 * Вынесена из SmtpCodeDeliveryChannel.cpp в отдельный файл, когда
 * появился второй потребитель (правило проекта: хэлпер, нужный больше
 * чем в одном месте, живёт в своём файле, а не как приватная деталь
 * первого потребителя).
 */
class TlsConnection {
public:
    TlsConnection(const std::string& host, int port);
    ~TlsConnection();

    TlsConnection(const TlsConnection&) = delete;
    TlsConnection& operator=(const TlsConnection&) = delete;

    [[nodiscard]] bool isConnected() const { return connected_; }

    void writeLine(const std::string& line);
    void writeRaw(const std::string& data);

    /// Читает одну строку ответа сервера (до \n), без завершающих \r\n.
    std::string readLine();

private:
    SSL_CTX* ctx_ = nullptr;
    BIO* bio_ = nullptr;
    bool connected_ = false;
};

}  // namespace auth_service
