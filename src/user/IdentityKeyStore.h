#pragma once

#include <QByteArray>
#include <QString>

#include <sodium.h>

namespace devicehub {

/// Генерирует и сохраняет локально долгоживущую пару ключей идентичности
/// X25519 этого аккаунта (issue #136, E2E-шифрование, Phase 1) —
/// приватный ключ никогда не покидает это хранилище; отправлять куда-либо
/// предназначен только publicKeyBase64() (через
/// UserProfileClient::publishPublicKey()).
///
/// Одна пара ключей на логин, хранится в каталоге, переданном
/// вызывающим кодом (обычно QStandardPaths::AppDataLocation — передаётся
/// снаружи, а не захардкожен, чтобы тесты могли указать на временный
/// каталог). Потеря этого файла означает потерю доступа ко всему,
/// зашифрованному для этой идентичности: на этом этапе нет поддержки
/// резервного копирования/восстановления/нескольких устройств (см.
/// issue #136).
class IdentityKeyStore {
public:
    /// Загружает пару ключей для @p login из @p storageDir, при первом
    /// использовании генерируя и сохраняя новую.
    IdentityKeyStore(QString storageDir, QString login);

    /// Публичный ключ X25519 в base64 — безопасно публиковать.
    [[nodiscard]] QString publicKeyBase64() const;

    /// Сырой 32-байтовый секретный ключ X25519 — для будущих локальных
    /// операций расшифровки/распечатывания (E2E Phase 2+); никогда не
    /// отправляется по сети.
    [[nodiscard]] QByteArray secretKeyBytes() const;

private:
    QString storageDir_;
    QString login_;
    unsigned char publicKey_[crypto_box_PUBLICKEYBYTES] = {};
    unsigned char secretKey_[crypto_box_SECRETKEYBYTES] = {};

    [[nodiscard]] QString keyFilePath() const;
    /// @return True, если существующий файл ключа был найден и загружен.
    bool loadIfExists();
    void generateAndPersist();
};

}  // namespace devicehub
