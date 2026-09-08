import { describe, expect, it } from "vitest";

import {
  decryptMessage,
  encryptMessage,
  generateChannelKey,
  unwrapKey,
  wrapKeyForRecipient,
} from "../../src/crypto/channelCrypto.js";
import { getSodium } from "../../src/crypto/sodium.js";

describe("channelCrypto", () => {
  it("round-trips a message through encrypt/decrypt with the same key", async () => {
    const channelKey = await generateChannelKey();
    const ciphertext = await encryptMessage("hello, channel", channelKey);
    expect(await decryptMessage(ciphertext, channelKey)).toBe("hello, channel");
  });

  it("produces a different nonce (and so a different ciphertext) each call", async () => {
    const channelKey = await generateChannelKey();
    const first = await encryptMessage("same text", channelKey);
    const second = await encryptMessage("same text", channelKey);
    expect(first).not.toBe(second);
  });

  it("fails to decrypt under the wrong key", async () => {
    const channelKey = await generateChannelKey();
    const wrongKey = await generateChannelKey();
    const ciphertext = await encryptMessage("secret", channelKey);
    expect(await decryptMessage(ciphertext, wrongKey)).toBeNull();
  });

  it("fails to decrypt malformed ciphertext", async () => {
    const channelKey = await generateChannelKey();
    expect(await decryptMessage("not-valid-base64!!", channelKey)).toBeNull();
    expect(await decryptMessage("", channelKey)).toBeNull();
  });

  it("round-trips a channel key through wrap/unwrap for the intended recipient", async () => {
    const sodium = await getSodium();
    const recipient = sodium.crypto_box_keypair();
    const channelKey = await generateChannelKey();

    const wrapped = await wrapKeyForRecipient(channelKey, recipient.publicKey);
    const unwrapped = await unwrapKey(wrapped, recipient.publicKey, recipient.privateKey);

    expect(unwrapped).not.toBeNull();
    expect(Array.from(unwrapped ?? [])).toEqual(Array.from(channelKey));
  });

  it("fails to unwrap a key sealed for someone else", async () => {
    const sodium = await getSodium();
    const recipient = sodium.crypto_box_keypair();
    const stranger = sodium.crypto_box_keypair();
    const channelKey = await generateChannelKey();

    const wrapped = await wrapKeyForRecipient(channelKey, recipient.publicKey);
    expect(await unwrapKey(wrapped, stranger.publicKey, stranger.privateKey)).toBeNull();
  });
});
