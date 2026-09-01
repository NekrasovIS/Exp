#include "user/IdentityKeyStore.h"

#include <QDir>
#include <QFile>

#include <stdexcept>

namespace devicehub {

namespace {

void ensureSodiumInitialized() {
    // sodium_init() безопасно вызывать многократно; защищаем статической
    // локальной переменной, чтобы конкурентные вызывающие платили за это
    // только один раз (C++11 гарантирует потокобезопасную инициализацию
    // локальных статических переменных функции) — та же идиома, что в
    // services/user-service/src/password_hash.cpp.
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
    // Один файл на логин: машина может хранить ключи идентичности более
    // чем для одного аккаунта (например, при переключении пользователей
    // без предварительного выхода).
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
        // Неверный размер — повреждённый или чужой файл. Считаем
        // отсутствующим, а не падаем; будет сгенерирована новая пара
        // ключей, которая его перезапишет.
        return false;
    }
    std::copy(bytes.begin(), bytes.end(), secretKey_);
    // Публичный ключ детерминированно выводится из секретного (умножение
    // на базовую точку X25519), а не тоже сохраняется — на одну вещь
    // меньше, которая могла бы разойтись с секретной половиной.
    crypto_scalarmult_base(publicKey_, secretKey_);
    return true;
}

void IdentityKeyStore::generateAndPersist() {
    crypto_box_keypair(publicKey_, secretKey_);

    QDir().mkpath(storageDir_);
    QFile file(keyFilePath());
    file.open(QIODevice::WriteOnly | QIODevice::Truncate);
    file.write(reinterpret_cast<const char*>(secretKey_), sizeof(secretKey_));
    file.close();
    // Только best-effort: кроссплатформенные биты прав доступа Qt не
    // отображаются на настоящие Windows ACL (там нет ограничения для
    // других локальных аккаунтов), в отличие от POSIX, где это реально
    // ограничивает файл владельцем. Задокументированное ограничение для
    // этого этапа (см. issue #136).
    file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
}

QString IdentityKeyStore::publicKeyBase64() const {
    return QByteArray(reinterpret_cast<const char*>(publicKey_), sizeof(publicKey_)).toBase64();
}

QByteArray IdentityKeyStore::secretKeyBytes() const {
    return QByteArray(reinterpret_cast<const char*>(secretKey_), sizeof(secretKey_));
}

}  // namespace devicehub
