import { renderHook, waitFor } from "@testing-library/react";
import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";

import { generateChannelKey, wrapKeyForRecipient } from "../../src/crypto/channelCrypto.js";
import { getSodium } from "../../src/crypto/sodium.js";
import { useChannelKey } from "../../src/crypto/useChannelKey.js";
import { SessionProvider } from "../../src/session/SessionContext.js";
import { fakeToken, jsonResponse, routedFetch } from "../testUtils.js";

const kStorageKey = "devicehub.web.session";
const kLogin = "alice";

beforeEach(() => {
  localStorage.setItem(
    kStorageKey,
    JSON.stringify({
      token: fakeToken(kLogin),
      refreshToken: "refresh-token",
      expiresAt: Math.floor(Date.now() / 1000) + 3600,
    }),
  );
});

afterEach(() => {
  vi.unstubAllGlobals();
  localStorage.clear();
});

describe("useChannelKey", () => {
  it("stays null while identityKeys hasn't loaded yet", () => {
    vi.stubGlobal("fetch", vi.fn());

    const { result } = renderHook(() => useChannelKey(7, null), { wrapper: SessionProvider });

    expect(result.current).toEqual({ loading: true, channelKey: null });
  });

  it("resolves to null (not an error) when the server has no wrapped key on file", async () => {
    const sodium = await getSodium();
    const identity = sodium.crypto_box_keypair();
    // Built once, outside the renderHook callback: a fresh object
    // literal recreated on every render would change identity by
    // reference each time, and since it's a useEffect dependency in
    // useChannelKey, that re-triggers the fetch/unwrap effect forever.
    const identityKeys = { ownPublicKey: identity.publicKey, ownSecretKey: identity.privateKey };
    vi.stubGlobal("fetch", routedFetch([[/\/channels\/7\/keys\/me/, () => jsonResponse(404, {})]]));

    const { result } = renderHook(() => useChannelKey(7, identityKeys), { wrapper: SessionProvider });

    await waitFor(() => expect(result.current.loading).toBe(false));
    expect(result.current.channelKey).toBeNull();
  });

  it("fetches and unwraps the caller's wrapped key", async () => {
    const sodium = await getSodium();
    const identity = sodium.crypto_box_keypair();
    const identityKeys = { ownPublicKey: identity.publicKey, ownSecretKey: identity.privateKey };
    const channelKey = await generateChannelKey();
    const wrapped = await wrapKeyForRecipient(channelKey, identity.publicKey);
    vi.stubGlobal(
      "fetch",
      routedFetch([[/\/channels\/7\/keys\/me/, () => jsonResponse(200, { wrapped_key: wrapped })]]),
    );

    const { result } = renderHook(() => useChannelKey(7, identityKeys), { wrapper: SessionProvider });

    await waitFor(() => expect(result.current.loading).toBe(false));
    expect(Array.from(result.current.channelKey ?? [])).toEqual(Array.from(channelKey));
  });

  it("resolves to null when the wrapped key can't be unwrapped by this identity", async () => {
    const sodium = await getSodium();
    const owner = sodium.crypto_box_keypair();
    const stranger = sodium.crypto_box_keypair();
    const identityKeys = { ownPublicKey: stranger.publicKey, ownSecretKey: stranger.privateKey };
    const channelKey = await generateChannelKey();
    const wrapped = await wrapKeyForRecipient(channelKey, owner.publicKey);
    vi.stubGlobal(
      "fetch",
      routedFetch([[/\/channels\/7\/keys\/me/, () => jsonResponse(200, { wrapped_key: wrapped })]]),
    );

    const { result } = renderHook(() => useChannelKey(7, identityKeys), { wrapper: SessionProvider });

    await waitFor(() => expect(result.current.loading).toBe(false));
    expect(result.current.channelKey).toBeNull();
  });
});
