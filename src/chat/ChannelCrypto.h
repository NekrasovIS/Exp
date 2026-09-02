#pragma once

#include <QByteArray>
#include <QString>

#include <optional>

namespace devicehub::channel_crypto {

/// Генерирует свежий случайный симметричный ключ для зашифрованного
/// канала (issue #138) — crypto_secretbox_KEYBYTES (32) случайных
/// байта. Вызывается один раз тем, кто создаёт канал; никогда не
/// переиспользуется между каналами.
[[nodiscard]] QByteArray generateChannelKey();

/// Шифрует @p plaintext с помощью @p channelKey (libsodium
/// crypto_secretbox, случайный nonce на каждый вызов). @return
/// base64(nonce || ciphertext) — nonce передаётся вместе с шифротекстом,
/// поскольку он не секретен, требуется лишь его уникальность на
/// сообщение при одном и том же ключе.
[[nodiscard]] QString encryptMessage(const QString& plaintext, const QByteArray& channelKey);

/// Обращает encryptMessage(). @return std::nullopt, если
/// @p ciphertextBase64 некорректен (неверная длина, невалидный base64)
/// или не проходит аутентификацию под @p channelKey (неверный ключ, или
/// байты были подделаны) — crypto_secretbox является AEAD-конструкцией,
/// поэтому неверный ключ не приводит к молчаливому получению
/// бессмысленного plaintext, а даёт явный отказ.
[[nodiscard]] std::optional<QString> decryptMessage(const QString& ciphertextBase64, const QByteArray& channelKey);

/// Запечатывает @p channelKey так, что восстановить его может только
/// обладатель соответствующего секретного ключа X25519 (другой половины
/// @p recipientPublicKey) (libsodium crypto_box_seal — анонимное
/// шифрование с открытым ключом, не нужен ни обратный канал, ни общая
/// сессия). @return base64 — именно это хранится на сервере как
/// "wrapped_key" одного из участников (см. таблицу channel_keys в
/// chat-service).
[[nodiscard]] QString wrapKeyForRecipient(const QByteArray& channelKey, const QByteArray& recipientPublicKey);

/// Обращает wrapKeyForRecipient(), используя собственную пару ключей
/// идентичности вызывающего (см. IdentityKeyStore). @return
/// std::nullopt, если @p wrappedKeyBase64 некорректен или не был
/// запечатан именно для этой пары ключей.
[[nodiscard]] std::optional<QByteArray> unwrapKey(const QString& wrappedKeyBase64, const QByteArray& ownPublicKey,
                                                   const QByteArray& ownSecretKey);

}  // namespace devicehub::channel_crypto
