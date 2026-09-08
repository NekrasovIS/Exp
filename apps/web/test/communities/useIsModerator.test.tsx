import { renderHook, waitFor } from "@testing-library/react";
import React from "react";
import { afterEach, describe, expect, it, vi } from "vitest";

import { useIsModerator } from "../../src/communities/useIsModerator.js";
import { SessionProvider } from "../../src/session/SessionContext.js";
import { jsonResponse } from "../testUtils.js";

const kStorageKey = "devicehub.web.session";

function tokenFor(login: string): string {
  const payload = btoa(JSON.stringify({ sub: login, exp: 9999999999 }))
    .replaceAll("+", "-")
    .replaceAll("/", "_")
    .replace(/=+$/, "");
  return `${payload}.sig`;
}

function Wrapper({ children }: { children: React.ReactNode }) {
  return <SessionProvider>{children}</SessionProvider>;
}

afterEach(() => {
  vi.unstubAllGlobals();
  localStorage.clear();
});

describe("useIsModerator", () => {
  it("is true when the current login is in the moderators list", async () => {
    localStorage.setItem(
      kStorageKey,
      JSON.stringify({ token: tokenFor("alice"), refreshToken: "r1", expiresAt: 9999999999 }),
    );
    vi.stubGlobal("fetch", vi.fn().mockResolvedValue(jsonResponse(200, ["alice", "bob"])));

    const { result } = renderHook(() => useIsModerator(1), { wrapper: Wrapper });

    await waitFor(() => expect(result.current).toBe(true));
  });

  it("is false when the current login is absent from the moderators list", async () => {
    localStorage.setItem(
      kStorageKey,
      JSON.stringify({ token: tokenFor("carol"), refreshToken: "r1", expiresAt: 9999999999 }),
    );
    vi.stubGlobal("fetch", vi.fn().mockResolvedValue(jsonResponse(200, ["alice", "bob"])));

    const { result } = renderHook(() => useIsModerator(1), { wrapper: Wrapper });

    await waitFor(() => expect(result.current).toBe(false));
  });

  it("is false while no community is selected, and makes no request", () => {
    localStorage.setItem(
      kStorageKey,
      JSON.stringify({ token: tokenFor("alice"), refreshToken: "r1", expiresAt: 9999999999 }),
    );
    const fetchSpy = vi.fn();
    vi.stubGlobal("fetch", fetchSpy);

    const { result } = renderHook(() => useIsModerator(null), { wrapper: Wrapper });

    expect(result.current).toBe(false);
    expect(fetchSpy).not.toHaveBeenCalled();
  });
});
