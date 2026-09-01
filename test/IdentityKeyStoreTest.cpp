#include "user/IdentityKeyStore.h"

#include <gtest/gtest.h>

#include <sodium.h>

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

namespace devicehub {
namespace {

TEST(IdentityKeyStoreTest, GeneratesA32ByteX25519PublicKeyOnFirstUse) {
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());

    const IdentityKeyStore store(dir.path(), QStringLiteral("alice"));
    const QByteArray decoded = QByteArray::fromBase64(store.publicKeyBase64().toUtf8());
    EXPECT_EQ(decoded.size(), 32);
    EXPECT_EQ(store.secretKeyBytes().size(), 32);
}

TEST(IdentityKeyStoreTest, ReloadingTheSameLoginReturnsTheSameKeyPair) {
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());

    const QString publicKeyA = IdentityKeyStore(dir.path(), QStringLiteral("alice")).publicKeyBase64();
    const QString publicKeyB = IdentityKeyStore(dir.path(), QStringLiteral("alice")).publicKeyBase64();

    EXPECT_EQ(publicKeyA, publicKeyB);
}

TEST(IdentityKeyStoreTest, DifferentLoginsGetDifferentKeyPairs) {
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());

    const QString aliceKey = IdentityKeyStore(dir.path(), QStringLiteral("alice")).publicKeyBase64();
    const QString bobKey = IdentityKeyStore(dir.path(), QStringLiteral("bob")).publicKeyBase64();

    EXPECT_NE(aliceKey, bobKey);
}

TEST(IdentityKeyStoreTest, PublicKeyIsDerivedFromTheStoredSecretKeyOnLoad) {
    // Plants a known secret key file directly (bypassing generation) and
    // checks the loaded public key matches independently computing it via
    // libsodium's own base-point multiplication — confirms
    // IdentityKeyStore::loadIfExists() actually derives from what's on
    // disk rather than, say, silently regenerating a fresh keypair.
    ASSERT_GE(sodium_init(), 0);
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());

    unsigned char knownSecretKey[crypto_box_SECRETKEYBYTES];
    randombytes_buf(knownSecretKey, sizeof(knownSecretKey));
    unsigned char expectedPublicKey[crypto_box_PUBLICKEYBYTES];
    crypto_scalarmult_base(expectedPublicKey, knownSecretKey);

    QFile file(QDir(dir.path()).filePath(QStringLiteral("dave.identity-key")));
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    file.write(reinterpret_cast<const char*>(knownSecretKey), sizeof(knownSecretKey));
    file.close();

    const IdentityKeyStore store(dir.path(), QStringLiteral("dave"));
    const QByteArray expected(reinterpret_cast<const char*>(expectedPublicKey), sizeof(expectedPublicKey));
    EXPECT_EQ(QByteArray::fromBase64(store.publicKeyBase64().toUtf8()), expected);
    EXPECT_EQ(store.secretKeyBytes(),
              QByteArray(reinterpret_cast<const char*>(knownSecretKey), sizeof(knownSecretKey)));
}

}  // namespace
}  // namespace devicehub
