// TypeScript port of src/chat/ChannelCrypto.h/.cpp (issue #269) — same
// algorithms, same byte layouts (crypto_secretbox_easy for messages,
// crypto_box_seal for wrapping a channel key to a recipient's identity
// public key), via the real libsodium (see sodium.ts) so keys/messages
// interop with DeviceHub's native C++ client. Every function is async
// only because loading the WASM module is; the actual crypto calls
// underneath are synchronous once that resolves.

import { getSodium } from "./sodium.js";

/** Fresh random symmetric key for a newly-created encrypted channel
 * (issue #138) — crypto_secretbox_KEYBYTES (32) bytes. Called once by
 * whoever creates the channel; never reused across channels. */
export async function generateChannelKey(): Promise<Uint8Array> {
  const sodium = await getSodium();
  return sodium.crypto_secretbox_keygen();
}

/** Encrypts @p plaintext with @p channelKey (crypto_secretbox, a fresh
 * random nonce per call). @returns base64(nonce || ciphertext) — the
 * nonce travels alongside the ciphertext since it isn't secret, only
 * required to be unique per message under the same key. */
export async function encryptMessage(plaintext: string, channelKey: Uint8Array): Promise<string> {
  const sodium = await getSodium();
  if (channelKey.length !== sodium.crypto_secretbox_KEYBYTES) {
    return "";
  }
  const nonce = sodium.randombytes_buf(sodium.crypto_secretbox_NONCEBYTES);
  // from_string() builds its result via the global TextEncoder, which
  // (unlike the library's own buffer allocations) isn't tied to
  // whichever realm's Uint8Array happens to be current — under jsdom
  // that's a different class than the one crypto_secretbox_easy's own
  // `instanceof Uint8Array` check expects, so re-wrap to normalize it.
  const message = new Uint8Array(sodium.from_string(plaintext));
  const ciphertext = sodium.crypto_secretbox_easy(message, nonce, channelKey);
  const combined = new Uint8Array(nonce.length + ciphertext.length);
  combined.set(nonce, 0);
  combined.set(ciphertext, nonce.length);
  return sodium.to_base64(combined, sodium.base64_variants.ORIGINAL);
}

/** Reverses encryptMessage(). @returns null if @p ciphertextBase64 is
 * malformed (wrong length, invalid base64) or fails to authenticate
 * under @p channelKey (wrong key, or the bytes were tampered with) —
 * crypto_secretbox is an AEAD construction, so a wrong key never
 * silently yields garbage plaintext, only an explicit failure. */
export async function decryptMessage(
  ciphertextBase64: string,
  channelKey: Uint8Array,
): Promise<string | null> {
  const sodium = await getSodium();
  if (channelKey.length !== sodium.crypto_secretbox_KEYBYTES) {
    return null;
  }
  let combined: Uint8Array;
  try {
    combined = sodium.from_base64(ciphertextBase64, sodium.base64_variants.ORIGINAL);
  } catch {
    return null;
  }
  if (combined.length < sodium.crypto_secretbox_NONCEBYTES + sodium.crypto_secretbox_MACBYTES) {
    return null;
  }
  const nonce = combined.slice(0, sodium.crypto_secretbox_NONCEBYTES);
  const ciphertext = combined.slice(sodium.crypto_secretbox_NONCEBYTES);
  try {
    const plaintext = sodium.crypto_secretbox_open_easy(ciphertext, nonce, channelKey);
    return sodium.to_string(plaintext);
  } catch {
    return null;
  }
}

/** Seals @p channelKey so only the holder of the matching X25519
 * secret key (the other half of @p recipientPublicKey) can recover it
 * (crypto_box_seal — anonymous public-key encryption, no return
 * channel or shared session needed). @returns base64 — exactly what
 * gets stored server-side as one member's "wrapped_key" (see the
 * channel_keys table in chat-service). */
export async function wrapKeyForRecipient(
  channelKey: Uint8Array,
  recipientPublicKey: Uint8Array,
): Promise<string> {
  const sodium = await getSodium();
  if (recipientPublicKey.length !== sodium.crypto_box_PUBLICKEYBYTES) {
    return "";
  }
  const sealed = sodium.crypto_box_seal(channelKey, recipientPublicKey);
  return sodium.to_base64(sealed, sodium.base64_variants.ORIGINAL);
}

/** Reverses wrapKeyForRecipient(), using the caller's own identity
 * keypair (see useIdentityKeys.ts). @returns null if
 * @p wrappedKeyBase64 is malformed or wasn't sealed for this exact
 * keypair. */
export async function unwrapKey(
  wrappedKeyBase64: string,
  ownPublicKey: Uint8Array,
  ownSecretKey: Uint8Array,
): Promise<Uint8Array | null> {
  const sodium = await getSodium();
  if (
    ownPublicKey.length !== sodium.crypto_box_PUBLICKEYBYTES ||
    ownSecretKey.length !== sodium.crypto_box_SECRETKEYBYTES
  ) {
    return null;
  }
  let sealed: Uint8Array;
  try {
    sealed = sodium.from_base64(wrappedKeyBase64, sodium.base64_variants.ORIGINAL);
  } catch {
    return null;
  }
  try {
    return sodium.crypto_box_seal_open(sealed, ownPublicKey, ownSecretKey);
  } catch {
    return null;
  }
}
