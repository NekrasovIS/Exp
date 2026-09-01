#pragma once

#include <QByteArray>
#include <QString>

#include <sodium.h>

namespace devicehub {

/// Generates and persists this account's long-term X25519 identity
/// keypair locally (issue #136, E2E encryption Phase 1) — the private
/// key never leaves this store; only publicKeyBase64() is meant to be
/// sent anywhere (via UserProfileClient::publishPublicKey()).
///
/// One keypair per login, stored under a caller-supplied directory
/// (normally QStandardPaths::AppDataLocation — passed in rather than
/// hardcoded so tests can point it at a temp directory). Losing this
/// file means losing access to anything encrypted to this identity: no
/// backup/recovery/multi-device support in this phase (see issue #136).
class IdentityKeyStore {
public:
    /// Loads the keypair for @p login from @p storageDir, generating and
    /// persisting a new one on first use.
    IdentityKeyStore(QString storageDir, QString login);

    /// Base64-encoded X25519 public key — safe to publish.
    [[nodiscard]] QString publicKeyBase64() const;

    /// Raw 32-byte X25519 secret key — for future local decrypt/unwrap
    /// operations (E2E Phase 2+); never sent over the network.
    [[nodiscard]] QByteArray secretKeyBytes() const;

private:
    QString storageDir_;
    QString login_;
    unsigned char publicKey_[crypto_box_PUBLICKEYBYTES] = {};
    unsigned char secretKey_[crypto_box_SECRETKEYBYTES] = {};

    [[nodiscard]] QString keyFilePath() const;
    /// @return True if an existing key file was found and loaded.
    bool loadIfExists();
    void generateAndPersist();
};

}  // namespace devicehub
