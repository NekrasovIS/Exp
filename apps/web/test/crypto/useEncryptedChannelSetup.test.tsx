import { renderHook } from "@testing-library/react";
import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";

import { getSodium } from "../../src/crypto/sodium.js";
import { useEncryptedChannelSetup } from "../../src/crypto/useEncryptedChannelSetup.js";
import { SessionProvider } from "../../src/session/SessionContext.js";
import { fakeToken, jsonResponse } from "../testUtils.js";

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

describe("useEncryptedChannelSetup", () => {
  it("wraps the channel key for itself and every member with a published key", async () => {
    const sodium = await getSodium();
    const own = sodium.crypto_box_keypair();
    const bob = sodium.crypto_box_keypair();

    const setChannelKeyCalls: Array<{ login: string; url: string }> = [];
    const fetchSpy = vi.fn(async (url: string, init?: RequestInit) => {
      if (/\/communities\/1\/members/.test(url)) {
        return jsonResponse(200, [kLogin, "bob", "carol"]);
      }
      if (/\/users\/bob\/profile/.test(url)) {
        return jsonResponse(200, {
          login: "bob",
          public_key: sodium.to_base64(bob.publicKey, sodium.base64_variants.ORIGINAL),
        });
      }
      if (/\/users\/carol\/profile/.test(url)) {
        // carol hasn't published a key yet — must be skipped, not thrown.
        return jsonResponse(200, { login: "carol" });
      }
      if (/\/channels\/9\/keys\//.test(url) && init?.method === "PUT") {
        setChannelKeyCalls.push({ login: decodeURIComponent(url.split("/keys/")[1] ?? ""), url });
        return jsonResponse(200, {});
      }
      return jsonResponse(200, []);
    });
    vi.stubGlobal("fetch", fetchSpy);

    const { result } = renderHook(() => useEncryptedChannelSetup(), { wrapper: SessionProvider });

    const skipped = await result.current.setUpEncryptedChannel(9, 1, {
      ownPublicKey: own.publicKey,
      ownSecretKey: own.privateKey,
    });

    expect(skipped).toEqual(["carol"]);
    expect(setChannelKeyCalls.map((call) => call.login).sort()).toEqual(["alice", "bob"].sort());
  });
});
