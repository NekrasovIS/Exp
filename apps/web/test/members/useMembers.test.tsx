import { renderHook, waitFor } from "@testing-library/react";
import React from "react";
import { afterEach, describe, expect, it, vi } from "vitest";

import { useMembers } from "../../src/members/useMembers.js";
import { SessionProvider } from "../../src/session/SessionContext.js";
import { fakeToken, jsonResponse } from "../testUtils.js";

const kStorageKey = "devicehub.web.session";

function Wrapper({ children }: { children: React.ReactNode }) {
  return <SessionProvider>{children}</SessionProvider>;
}

afterEach(() => {
  vi.unstubAllGlobals();
  localStorage.clear();
});

describe("useMembers", () => {
  it("loads the member list for the given community", async () => {
    localStorage.setItem(
      kStorageKey,
      JSON.stringify({ token: fakeToken("alice"), refreshToken: "r1", expiresAt: 9999999999 }),
    );
    vi.stubGlobal("fetch", vi.fn().mockResolvedValue(jsonResponse(200, ["alice", "bob"])));

    const { result } = renderHook(() => useMembers(1), { wrapper: Wrapper });

    await waitFor(() => expect(result.current.loading).toBe(false));
    expect(result.current.members).toEqual(["alice", "bob"]);
  });

  it("is empty while no community is selected, and makes no request", () => {
    localStorage.setItem(
      kStorageKey,
      JSON.stringify({ token: fakeToken("alice"), refreshToken: "r1", expiresAt: 9999999999 }),
    );
    const fetchSpy = vi.fn();
    vi.stubGlobal("fetch", fetchSpy);

    const { result } = renderHook(() => useMembers(null), { wrapper: Wrapper });

    expect(result.current.members).toEqual([]);
    expect(fetchSpy).not.toHaveBeenCalled();
  });

  it("reports an error when the request fails", async () => {
    localStorage.setItem(
      kStorageKey,
      JSON.stringify({ token: fakeToken("alice"), refreshToken: "r1", expiresAt: 9999999999 }),
    );
    vi.stubGlobal("fetch", vi.fn().mockResolvedValue(jsonResponse(500, { error: "boom" })));

    const { result } = renderHook(() => useMembers(1), { wrapper: Wrapper });

    await waitFor(() => expect(result.current.loading).toBe(false));
    expect(result.current.error).not.toBeNull();
  });
});
