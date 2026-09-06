#include "chat/ChannelCrypto.h"
#include "user/IdentityKeyStore.h"

#include <gtest/gtest.h>

#include <QByteArray>
#include <QTemporaryDir>

namespace devicehub::channel_crypto {
namespace {

TEST(ChannelCryptoTest, GeneratedChannelKeyIs32Bytes) {
    EXPECT_EQ(generateChannelKey().size(), 32);
}

TEST(ChannelCryptoTest, TwoGeneratedChannelKeysDiffer) {
    EXPECT_NE(generateChannelKey(), generateChannelKey());
}

TEST(ChannelCryptoTest, EncryptThenDecryptRoundTripsThePlaintext) {
    const QByteArray key = generateChannelKey();
    const QString plaintext = QStringLiteral("hello, encrypted channel — issue #138");

    const QString ciphertext = encryptMessage(plaintext, key);
    EXPECT_FALSE(ciphertext.isEmpty());
    EXPECT_NE(ciphertext, plaintext);

    const std::optional<QString> decrypted = decryptMessage(ciphertext, key);
    ASSERT_TRUE(decrypted.has_value());
    EXPECT_EQ(*decrypted, plaintext);
}

TEST(ChannelCryptoTest, TwoEncryptionsOfTheSamePlaintextProduceDifferentCiphertext) {
    // Случайный nonce при каждом вызове — один и тот же открытый текст/ключ
    // не должны дважды давать идентичный шифротекст.
    const QByteArray key = generateChannelKey();
    const QString plaintext = QStringLiteral("same message");
    EXPECT_NE(encryptMessage(plaintext, key), encryptMessage(plaintext, key));
}

TEST(ChannelCryptoTest, DecryptWithWrongKeyFails) {
    const QByteArray key = generateChannelKey();
    const QByteArray wrongKey = generateChannelKey();
    const QString ciphertext = encryptMessage(QStringLiteral("secret"), key);

    EXPECT_FALSE(decryptMessage(ciphertext, wrongKey).has_value());
}

TEST(ChannelCryptoTest, DecryptOfGarbageBase64Fails) {
    const QByteArray key = generateChannelKey();
    EXPECT_FALSE(decryptMessage(QStringLiteral("not-valid-base64-ciphertext"), key).has_value());
}

TEST(ChannelCryptoTest, DecryptOfEmptyStringFails) {
    const QByteArray key = generateChannelKey();
    EXPECT_FALSE(decryptMessage(QString(), key).has_value());
}

TEST(ChannelCryptoTest, WrapThenUnwrapRoundTripsTheChannelKey) {
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const IdentityKeyStore recipient(dir.path(), QStringLiteral("alice"));

    const QByteArray channelKey = generateChannelKey();
    const QString wrapped = wrapKeyForRecipient(channelKey, QByteArray::fromBase64(recipient.publicKeyBase64().toUtf8()));
    EXPECT_FALSE(wrapped.isEmpty());

    const std::optional<QByteArray> unwrapped = unwrapKey(
        wrapped, QByteArray::fromBase64(recipient.publicKeyBase64().toUtf8()), recipient.secretKeyBytes());
    ASSERT_TRUE(unwrapped.has_value());
    EXPECT_EQ(*unwrapped, channelKey);
}

TEST(ChannelCryptoTest, UnwrapWithADifferentIdentityFails) {
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const IdentityKeyStore recipient(dir.path(), QStringLiteral("alice"));
    const IdentityKeyStore someoneElse(dir.path(), QStringLiteral("mallory"));

    const QByteArray channelKey = generateChannelKey();
    const QString wrapped = wrapKeyForRecipient(channelKey, QByteArray::fromBase64(recipient.publicKeyBase64().toUtf8()));

    const std::optional<QByteArray> unwrapped =
        unwrapKey(wrapped, QByteArray::fromBase64(someoneElse.publicKeyBase64().toUtf8()), someoneElse.secretKeyBytes());
    EXPECT_FALSE(unwrapped.has_value());
}

TEST(ChannelCryptoTest, UnwrapOfGarbageBase64Fails) {
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const IdentityKeyStore recipient(dir.path(), QStringLiteral("alice"));

    const std::optional<QByteArray> unwrapped =
        unwrapKey(QStringLiteral("garbage"), QByteArray::fromBase64(recipient.publicKeyBase64().toUtf8()),
                  recipient.secretKeyBytes());
    EXPECT_FALSE(unwrapped.has_value());
}

}  // namespace
}  // namespace devicehub::channel_crypto
