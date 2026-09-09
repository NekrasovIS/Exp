import { renderHook, waitFor } from "@testing-library/react";
import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";

import { useCommunities } from "../../src/communities/useCommunities.js";
import { setPendingInviteCode } from "../../src/communities/pendingInvite.js";
import { SessionProvider } from "../../src/session/SessionContext.js";
import { fakeToken, jsonResponse } from "../testUtils.js";

const kStorageKey = "devicehub.web.session";

function Wrapper({ children }: { children: React.ReactNode }) {
  return <SessionProvider>{children}</SessionProvider>;
}

beforeEach(() => {
  localStorage.setItem(
    kStorageKey,
    JSON.stringify({
      token: fakeToken("alice"),
      refreshToken: "refresh-token",
      expiresAt: Math.floor(Date.now() / 1000) + 3600,
    }),
  );
  sessionStorage.clear();
});

afterEach(() => {
  vi.unstubAllGlobals();
  localStorage.clear();
});

describe("useCommunities", () => {
  it("consumes a pending invite code left by JoinPage and joins that community", async () => {
    setPendingInviteCode("ABCD1234EF");
    const fetchSpy = vi
      .fn()
      .mockResolvedValueOnce(jsonResponse(200, []))
      .mockResolvedValueOnce(jsonResponse(200, { id: 1, name: "Robotics Club" }))
      .mockResolvedValueOnce(jsonResponse(200, [{ id: 1, name: "Robotics Club", owner: "bob" }]));
    vi.stubGlobal("fetch", fetchSpy);

    const { result } = renderHook(() => useCommunities(), { wrapper: Wrapper });

    await waitFor(() => expect(result.current.communities).toHaveLength(1));
    expect(fetchSpy).toHaveBeenCalledTimes(3);
    expect(JSON.parse((fetchSpy.mock.calls[1] as [string, RequestInit])[1].body as string)).toEqual({
      code: "ABCD1234EF",
    });
  });

  it("does nothing extra when there is no pending invite code", async () => {
    const fetchSpy = vi.fn().mockResolvedValueOnce(jsonResponse(200, []));
    vi.stubGlobal("fetch", fetchSpy);

    const { result } = renderHook(() => useCommunities(), { wrapper: Wrapper });

    await waitFor(() => expect(result.current.loading).toBe(false));
    expect(fetchSpy).toHaveBeenCalledTimes(1);
  });
});
