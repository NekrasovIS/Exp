// Issue #269 — libsodium-wrappers is a WASM build of the *same*
// libsodium DeviceHub links natively (src/chat/ChannelCrypto.cpp) —
// deliberately not the native WebCrypto API: WebCrypto has no
// crypto_box_seal/crypto_secretbox_easy equivalent (no XSalsa20-Poly1305,
// and browser X25519 support is too new/inconsistent to rely on), so a
// hand-rolled implementation on WebCrypto primitives couldn't produce
// byte-compatible output with the C++ client's wrapped keys/ciphertexts
// in the first place. Using the real libsodium via WASM sidesteps that
// entirely — same functions, same byte layouts, guaranteed interop.

import sodium from "libsodium-wrappers";

/** Resolves once the WASM module has finished loading — safe to await
 * repeatedly (sodium.ready itself is a cached promise), so every
 * exported function in channelCrypto.ts awaits this itself rather than
 * pushing a separate "did you call init()?" requirement onto callers. */
export async function getSodium(): Promise<typeof sodium> {
  await sodium.ready;
  return sodium;
}
