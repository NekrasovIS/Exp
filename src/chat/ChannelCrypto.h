#pragma once

#include <QByteArray>
#include <QString>

#include <optional>

namespace devicehub::channel_crypto {

/// Generates a fresh random symmetric key for an encrypted channel
/// (issue #138) — crypto_secretbox_KEYBYTES (32) random bytes. Called
/// once by whoever creates the channel; never reused across channels.
[[nodiscard]] QByteArray generateChannelKey();

/// Encrypts @p plaintext with @p channelKey (libsodium crypto_secretbox,
/// random nonce per call). @return base64(nonce || ciphertext) — the
/// nonce travels alongside the ciphertext since it isn't secret, only
/// required to be unique per message under the same key.
[[nodiscard]] QString encryptMessage(const QString& plaintext, const QByteArray& channelKey);

/// Reverses encryptMessage(). @return std::nullopt if @p ciphertextBase64
/// is malformed (wrong length, invalid base64) or fails authentication
/// under @p channelKey (wrong key, or the bytes were tampered with) —
/// crypto_secretbox is an AEAD construction, so a wrong key doesn't
/// silently produce garbage plaintext, it fails outright.
[[nodiscard]] std::optional<QString> decryptMessage(const QString& ciphertextBase64, const QByteArray& channelKey);

/// Seals @p channelKey so only the holder of the matching X25519 secret
/// key (@p recipientPublicKey's other half) can recover it (libsodium
/// crypto_box_seal — anonymous public-key encryption, no reply channel
/// or shared session needed). @return base64 — this is what gets stored
/// server-side as one member's "wrapped_key" (see chat-service's
/// channel_keys table).
[[nodiscard]] QString wrapKeyForRecipient(const QByteArray& channelKey, const QByteArray& recipientPublicKey);

/// Reverses wrapKeyForRecipient() using the caller's own identity
/// keypair (see IdentityKeyStore). @return std::nullopt if
/// @p wrappedKeyBase64 is malformed or wasn't sealed for this exact
/// keypair.
[[nodiscard]] std::optional<QByteArray> unwrapKey(const QString& wrappedKeyBase64, const QByteArray& ownPublicKey,
                                                   const QByteArray& ownSecretKey);

}  // namespace devicehub::channel_crypto
