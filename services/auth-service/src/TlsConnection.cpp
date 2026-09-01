#include "TlsConnection.h"

#include <openssl/bio.h>
#include <openssl/ssl.h>

namespace auth_service {

TlsConnection::TlsConnection(const std::string& host, int port) {
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

TlsConnection::~TlsConnection() {
    BIO_free_all(bio_);
    SSL_CTX_free(ctx_);
}

void TlsConnection::writeLine(const std::string& line) {
    const std::string withCrlf = line + "\r\n";
    BIO_write(bio_, withCrlf.data(), static_cast<int>(withCrlf.size()));
}

void TlsConnection::writeRaw(const std::string& data) {
    BIO_write(bio_, data.data(), static_cast<int>(data.size()));
}

std::string TlsConnection::readLine() {
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

}  // namespace auth_service
