#include "SmtpCodeDeliveryChannel.h"

#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/ssl.h>

#include <cstdlib>
#include <iostream>
#include <sstream>
#include <utility>

namespace auth_service {

namespace {

/// Тонкая обёртка над OpenSSL BIO для TLS-соединения по одной строке за
/// раз — ровно то, что нужно текстовому протоколу SMTP, без обвязки
/// внешней библиотеки вроде libcurl.
class TlsConnection {
public:
    TlsConnection(const std::string& host, int port) {
        ctx_ = SSL_CTX_new(TLS_client_method());
        bio_ = BIO_new_ssl_connect(ctx_);
        SSL* ssl = nullptr;
        BIO_get_ssl(bio_, &ssl);
        SSL_set_mode(ssl, SSL_MODE_AUTO_RETRY);
        SSL_set_tlsext_host_name(ssl, host.c_str());  // SNI

        const std::string hostPort = host + ":" + std::to_string(port);
        BIO_set_conn_hostname(bio_, hostPort.c_str());
        connected_ = BIO_do_connect(bio_) == 1 && BIO_do_handshake(bio_) == 1;
    }

    ~TlsConnection() {
        BIO_free_all(bio_);
        SSL_CTX_free(ctx_);
    }

    TlsConnection(const TlsConnection&) = delete;
    TlsConnection& operator=(const TlsConnection&) = delete;

    [[nodiscard]] bool isConnected() const { return connected_; }

    void writeLine(const std::string& line) {
        const std::string withCrlf = line + "\r\n";
        BIO_write(bio_, withCrlf.data(), static_cast<int>(withCrlf.size()));
    }

    void writeRaw(const std::string& data) { BIO_write(bio_, data.data(), static_cast<int>(data.size())); }

    /// Читает одну строку ответа сервера (до \n), без завершающих \r\n.
    std::string readLine() {
        std::string line;
        char ch = 0;
        while (BIO_read(bio_, &ch, 1) == 1) {
            if (ch == '\n') {
                break;
            }
            if (ch != '\r') {
                line.push_back(ch);
            }
        }
        return line;
    }

private:
    SSL_CTX* ctx_ = nullptr;
    BIO* bio_ = nullptr;
    bool connected_ = false;
};

/// Ответ SMTP-сервера может занимать несколько строк ("250-первая",
/// "250-вторая", "250 последняя") — код повторяется на каждой, дефис
/// вместо пробела после кода значит "будет ещё строка".
int readSmtpResponseCode(TlsConnection& connection) {
    std::string lastLine;
    for (;;) {
        lastLine = connection.readLine();
        if (lastLine.size() < 4 || lastLine[3] != '-') {
            break;
        }
    }
    return lastLine.size() >= 3 ? std::atoi(lastLine.substr(0, 3).c_str()) : 0;
}

bool expectClass2(TlsConnection& connection) {
    return readSmtpResponseCode(connection) / 100 == 2;
}

bool expectExact(TlsConnection& connection, int code) {
    return readSmtpResponseCode(connection) == code;
}

std::string base64Encode(const std::string& input) {
    const int encodedLen = 4 * ((static_cast<int>(input.size()) + 2) / 3);
    std::string output(static_cast<std::size_t>(encodedLen), '\0');
    const int written =
        EVP_EncodeBlock(reinterpret_cast<unsigned char*>(output.data()),
                         reinterpret_cast<const unsigned char*>(input.data()), static_cast<int>(input.size()));
    output.resize(static_cast<std::size_t>(written));
    return output;
}

/// Dot-stuffing (RFC 5321 §4.5.2): строка, начинающаяся с точки,
/// получает вторую точку в начале — иначе SMTP-сервер принял бы её за
/// маркер конца DATA ("\r\n.\r\n").
std::string dotStuff(const std::string& message) {
    std::string result;
    bool atLineStart = true;
    for (char ch : message) {
        if (atLineStart && ch == '.') {
            result.push_back('.');
        }
        result.push_back(ch);
        atLineStart = (ch == '\n');
    }
    return result;
}

std::string buildMessage(const std::string& from, const std::string& to, const std::string& code) {
    std::ostringstream out;
    out << "From: " << from << "\r\n"
        << "To: " << to << "\r\n"
        << "Subject: Your DeviceHub sign-in code\r\n"
        << "Content-Type: text/plain; charset=utf-8\r\n"
        << "\r\n"
        << "Your one-time sign-in code is: " << code << "\r\n"
        << "It expires in a few minutes. If you didn't request this, you can ignore this email.\r\n";
    return out.str();
}

std::string envOrEmpty(const char* name) {
    const char* value = std::getenv(name);
    return value != nullptr ? std::string(value) : std::string();
}

}  // namespace

SmtpCodeDeliveryChannel::SmtpCodeDeliveryChannel(SmtpConfig config) : config_(std::move(config)) {}

void SmtpCodeDeliveryChannel::send(const std::string& destination, const std::string& code) const {
    // Не бросаем исключение на любом шаге ниже — тот же принцип "fail
    // closed, не роняем обработчик запроса", что и у UserServiceClient;
    // /auth/otp/request всё равно всегда отвечает 200 независимо от
    // результата доставки (см. HttpServer — не палим существование
    // аккаунта через ответ).
    const auto fail = [&destination](const char* step) {
        std::cerr << "[OTP] SMTP-шаг \"" << step << "\" завершился ошибкой при отправке кода на " << destination
                   << std::endl;
    };

    TlsConnection connection(config_.host, config_.port);
    if (!connection.isConnected()) {
        fail("TLS-соединение");
        return;
    }

    readSmtpResponseCode(connection);  // приветствие сервера (220 ...)

    connection.writeLine("EHLO devicehub-auth-service");
    if (!expectClass2(connection)) {
        fail("EHLO");
        return;
    }

    if (!config_.username.empty()) {
        connection.writeLine("AUTH LOGIN");
        if (!expectExact(connection, 334)) {
            fail("AUTH LOGIN");
            return;
        }
        connection.writeLine(base64Encode(config_.username));
        if (!expectExact(connection, 334)) {
            fail("AUTH LOGIN (логин)");
            return;
        }
        connection.writeLine(base64Encode(config_.password));
        if (!expectClass2(connection)) {
            fail("AUTH LOGIN (пароль)");
            return;
        }
    }

    connection.writeLine("MAIL FROM:<" + config_.fromAddress + ">");
    if (!expectClass2(connection)) {
        fail("MAIL FROM");
        return;
    }

    connection.writeLine("RCPT TO:<" + destination + ">");
    if (!expectClass2(connection)) {
        fail("RCPT TO");
        return;
    }

    connection.writeLine("DATA");
    if (!expectExact(connection, 354)) {
        fail("DATA");
        return;
    }

    connection.writeRaw(dotStuff(buildMessage(config_.fromAddress, destination, code)));
    connection.writeLine(".");
    if (!expectClass2(connection)) {
        fail("тело письма");
        return;
    }

    connection.writeLine("QUIT");
}

std::optional<SmtpConfig> SmtpCodeDeliveryChannel::fromEnvironment() {
    const std::string host = envOrEmpty("SMTP_HOST");
    if (host.empty()) {
        return std::nullopt;
    }

    SmtpConfig config{.host = host, .username = envOrEmpty("SMTP_USERNAME"), .password = envOrEmpty("SMTP_PASSWORD")};
    if (const std::string port = envOrEmpty("SMTP_PORT"); !port.empty()) {
        config.port = std::atoi(port.c_str());
    }
    config.fromAddress = envOrEmpty("SMTP_FROM");
    if (config.fromAddress.empty()) {
        config.fromAddress = config.username;
    }
    return config;
}

}  // namespace auth_service
