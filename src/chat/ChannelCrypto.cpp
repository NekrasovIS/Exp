#include "chat/ChannelCrypto.h"

#include <sodium.h>

#include <stdexcept>

namespace devicehub::channel_crypto {

namespace {

void ensureSodiumInitialized() {
    // Та же идиома, что в IdentityKeyStore.cpp / password_hash.cpp сервиса user-service.
    static const bool initialized = [] {
        if (sodium_init() < 0) {
            throw std::runtime_error("libsodium failed to initialize");
        }
        return true;
    }();
    static_cast<void>(initialized);
}

}  // namespace

QByteArray generateChannelKey() {
    ensureSodiumInitialized();
    QByteArray key(crypto_secretbox_KEYBYTES, Qt::Uninitialized);
    randombytes_buf(reinterpret_cast<unsigned char*>(key.data()), static_cast<std::size_t>(key.size()));
    return key;
}

QString encryptMessage(const QString& plaintext, const QByteArray& channelKey) {
    ensureSodiumInitialized();
    if (channelKey.size() != crypto_secretbox_KEYBYTES) {
        return QString();
    }

    const QByteArray plaintextBytes = plaintext.toUtf8();
    QByteArray output(crypto_secretbox_NONCEBYTES + crypto_secretbox_MACBYTES + plaintextBytes.size(),
                       Qt::Uninitialized);
    auto* nonce = reinterpret_cast<unsigned char*>(output.data());
    randombytes_buf(nonce, crypto_secretbox_NONCEBYTES);

    auto* ciphertext = reinterpret_cast<unsigned char*>(output.data()) + crypto_secretbox_NONCEBYTES;
    crypto_secretbox_easy(ciphertext, reinterpret_cast<const unsigned char*>(plaintextBytes.constData()),
                           static_cast<unsigned long long>(plaintextBytes.size()), nonce,
                           reinterpret_cast<const unsigned char*>(channelKey.constData()));

    return QString::fromLatin1(output.toBase64());
}

std::optional<QString> decryptMessage(const QString& ciphertextBase64, const QByteArray& channelKey) {
    ensureSodiumInitialized();
    if (channelKey.size() != crypto_secretbox_KEYBYTES) {
        return std::nullopt;
    }

    const QByteArray combined = QByteArray::fromBase64(ciphertextBase64.toUtf8());
    if (combined.size() < crypto_secretbox_NONCEBYTES + crypto_secretbox_MACBYTES) {
        return std::nullopt;
    }

    const auto* nonce = reinterpret_cast<const unsigned char*>(combined.constData());
    const auto* ciphertext = reinterpret_cast<const unsigned char*>(combined.constData()) + crypto_secretbox_NONCEBYTES;
    const auto ciphertextLength =
        static_cast<unsigned long long>(combined.size() - crypto_secretbox_NONCEBYTES);

    QByteArray plaintext(static_cast<qsizetype>(ciphertextLength - crypto_secretbox_MACBYTES), Qt::Uninitialized);
    if (crypto_secretbox_open_easy(reinterpret_cast<unsigned char*>(plaintext.data()), ciphertext, ciphertextLength,
                                    nonce, reinterpret_cast<const unsigned char*>(channelKey.constData())) != 0) {
        return std::nullopt;
    }
    return QString::fromUtf8(plaintext);
}

QString wrapKeyForRecipient(const QByteArray& channelKey, const QByteArray& recipientPublicKey) {
    ensureSodiumInitialized();
    if (recipientPublicKey.size() != crypto_box_PUBLICKEYBYTES) {
        return QString();
    }

    QByteArray output(crypto_box_SEALBYTES + channelKey.size(), Qt::Uninitialized);
    crypto_box_seal(reinterpret_cast<unsigned char*>(output.data()),
                     reinterpret_cast<const unsigned char*>(channelKey.constData()),
                     static_cast<unsigned long long>(channelKey.size()),
                     reinterpret_cast<const unsigned char*>(recipientPublicKey.constData()));
    return QString::fromLatin1(output.toBase64());
}

std::optional<QByteArray> unwrapKey(const QString& wrappedKeyBase64, const QByteArray& ownPublicKey,
                                     const QByteArray& ownSecretKey) {
    ensureSodiumInitialized();
    if (ownPublicKey.size() != crypto_box_PUBLICKEYBYTES || ownSecretKey.size() != crypto_box_SECRETKEYBYTES) {
        return std::nullopt;
    }

    const QByteArray sealed = QByteArray::fromBase64(wrappedKeyBase64.toUtf8());
    if (sealed.size() <= crypto_box_SEALBYTES) {
        return std::nullopt;
    }

    QByteArray channelKey(sealed.size() - crypto_box_SEALBYTES, Qt::Uninitialized);
    if (crypto_box_seal_open(reinterpret_cast<unsigned char*>(channelKey.data()),
                              reinterpret_cast<const unsigned char*>(sealed.constData()),
                              static_cast<unsigned long long>(sealed.size()),
                              reinterpret_cast<const unsigned char*>(ownPublicKey.constData()),
                              reinterpret_cast<const unsigned char*>(ownSecretKey.constData())) != 0) {
        return std::nullopt;
    }
    return channelKey;
}

}  // namespace devicehub::channel_crypto
