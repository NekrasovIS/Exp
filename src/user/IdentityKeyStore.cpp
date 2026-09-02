#include "user/IdentityKeyStore.h"

#include <QDir>
#include <QFile>

#include <stdexcept>

namespace devicehub {

namespace {

void ensureSodiumInitialized() {
    // sodium_init() is safe to call multiple times; guard with a static
    // local so concurrent callers only pay for it once (C++11 guarantees
    // thread-safe initialization of function-local statics) — same idiom
    // as services/user-service/src/password_hash.cpp.
    static const bool initialized = [] {
        if (sodium_init() < 0) {
            throw std::runtime_error("libsodium failed to initialize");
        }
        return true;
    }();
    static_cast<void>(initialized);
}

}  // namespace

IdentityKeyStore::IdentityKeyStore(QString storageDir, QString login)
    : storageDir_(std::move(storageDir)), login_(std::move(login)) {
    ensureSodiumInitialized();
    if (!loadIfExists()) {
        generateAndPersist();
    }
}

QString IdentityKeyStore::keyFilePath() const {
    // One file per login: a machine can hold identity keys for more than
    // one account (e.g. switching users without logging out first).
    return QDir(storageDir_).filePath(login_ + QStringLiteral(".identity-key"));
}

bool IdentityKeyStore::loadIfExists() {
    QFile file(keyFilePath());
    if (!file.exists()) {
        return false;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    const QByteArray bytes = file.readAll();
    if (bytes.size() != crypto_box_SECRETKEYBYTES) {
        // Wrong size — corrupt or foreign file. Treat as absent rather
        // than crash; a fresh keypair gets generated and overwrites it.
        return false;
    }
    std::copy(bytes.begin(), bytes.end(), secretKey_);
    // The public key is deterministically derived from the secret key
    // (X25519 base-point multiplication) rather than also persisted —
    // one fewer thing that could get out of sync with the secret half.
    crypto_scalarmult_base(publicKey_, secretKey_);
    return true;
}

void IdentityKeyStore::generateAndPersist() {
    crypto_box_keypair(publicKey_, secretKey_);

    QDir().mkpath(storageDir_);
    QFile file(keyFilePath());
    file.open(QIODevice::WriteOnly | QIODevice::Truncate);
    // secretKey_/publicKey_ — unsigned char[] из libsodium; QFile::write()/
    // конструктор QByteArray из Qt принимают char* — reinterpret_cast
    // здесь стандартный способ перейти через эту границу типов
    // указателей (та же логика применима к publicKeyBase64()/
    // secretKeyBytes() ниже), всегда в паре с sizeof(), поэтому чтение
    // через переинтерпретированный указатель никогда не выйдет за
    // границы массива.
    file.write(reinterpret_cast<const char*>(secretKey_), sizeof(secretKey_));
    file.close();
    // Best-effort only: Qt's cross-platform permission bits don't map to
    // real Windows ACLs (no restriction against other local accounts
    // there), unlike POSIX where this actually limits the file to the
    // owner. Documented limitation for this phase (see issue #136).
    file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
}

QString IdentityKeyStore::publicKeyBase64() const {
    return QByteArray(reinterpret_cast<const char*>(publicKey_), sizeof(publicKey_)).toBase64();
}

QByteArray IdentityKeyStore::secretKeyBytes() const {
    return QByteArray(reinterpret_cast<const char*>(secretKey_), sizeof(secretKey_));
}

}  // namespace devicehub
