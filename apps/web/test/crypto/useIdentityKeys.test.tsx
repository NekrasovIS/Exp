import { renderHook, waitFor } from "@testing-library/react";
import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";

import { getSodium } from "../../src/crypto/sodium.js";
import { useIdentityKeys } from "../../src/crypto/useIdentityKeys.js";
import { SessionProvider } from "../../src/session/SessionContext.js";
import { fakeToken, jsonResponse, routedFetch } from "../testUtils.js";

const kStorageKey = "devicehub.web.session";
const kLogin = "alice";

function seedSession(): void {
  localStorage.setItem(
    kStorageKey,
    JSON.stringify({
      token: fakeToken(kLogin),
      refreshToken: "refresh-token",
      expiresAt: Math.floor(Date.now() / 1000) + 3600,
    }),
  );
}

beforeEach(seedSession);

afterEach(() => {
  vi.unstubAllGlobals();
  localStorage.clear();
});

describe("useIdentityKeys", () => {
  it("generates a fresh keypair, persists it, and republishes the public half", async () => {
    const fetchSpy = routedFetch([[/\/users\/me/, () => jsonResponse(200, { login: kLogin })]]);
    vi.stubGlobal("fetch", fetchSpy);

    const { result } = renderHook(() => useIdentityKeys(), { wrapper: SessionProvider });

    await waitFor(() => expect(result.current).not.toBeNull());
    expect(result.current?.ownPublicKey).toHaveLength(32);
    expect(result.current?.ownSecretKey).toHaveLength(32);
    expect(localStorage.getItem(`devicehub.web.identityKeys.${kLogin}`)).not.toBeNull();
    expect(fetchSpy).toHaveBeenCalled();
  });

  it("reuses an already-stored keypair instead of generating a new one", async () => {
    const sodium = await getSodium();
    const stored = sodium.crypto_box_keypair();
    localStorage.setItem(
      `devicehub.web.identityKeys.${kLogin}`,
      JSON.stringify({
        publicKey: sodium.to_base64(stored.publicKey, sodium.base64_variants.ORIGINAL),
        secretKey: sodium.to_base64(stored.privateKey, sodium.base64_variants.ORIGINAL),
      }),
    );
    vi.stubGlobal("fetch", routedFetch([[/\/users\/me/, () => jsonResponse(200, { login: kLogin })]]));

    const { result } = renderHook(() => useIdentityKeys(), { wrapper: SessionProvider });

    await waitFor(() => expect(result.current).not.toBeNull());
    expect(Array.from(result.current?.ownPublicKey ?? [])).toEqual(Array.from(stored.publicKey));
  });
});
