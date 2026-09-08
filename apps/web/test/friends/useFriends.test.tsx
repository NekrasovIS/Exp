import { act, renderHook, waitFor } from "@testing-library/react";
import React from "react";
import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";

import { useFriends } from "../../src/friends/useFriends.js";
import { SessionProvider } from "../../src/session/SessionContext.js";
import { jsonResponse } from "../testUtils.js";

const kStorageKey = "devicehub.web.session";

function Wrapper({ children }: { children: React.ReactNode }) {
  return <SessionProvider>{children}</SessionProvider>;
}

beforeEach(() => {
  localStorage.setItem(
    kStorageKey,
    JSON.stringify({
      token: "access-token",
      refreshToken: "refresh-token",
      expiresAt: Math.floor(Date.now() / 1000) + 3600,
    }),
  );
});

afterEach(() => {
  vi.unstubAllGlobals();
  localStorage.clear();
});

describe("useFriends", () => {
  it("loads friends and incoming requests", async () => {
    vi.stubGlobal(
      "fetch",
      vi.fn().mockImplementation((url: string) => {
        if (url.endsWith("/friends")) {
          return Promise.resolve(jsonResponse(200, ["bob"]));
        }
        return Promise.resolve(
          jsonResponse(200, [{ id: 1, requester_login: "carol", created_at: "2026-01-01T00:00:00Z" }]),
        );
      }),
    );
    const { result } = renderHook(() => useFriends(), { wrapper: Wrapper });

    await waitFor(() => expect(result.current.loading).toBe(false));
    expect(result.current.friends).toEqual(["bob"]);
    expect(result.current.incomingRequests).toEqual([
      { id: 1, requesterLogin: "carol", createdAt: "2026-01-01T00:00:00Z" },
    ]);
  });

  it("acceptRequest calls the server then refreshes", async () => {
    const fetchSpy = vi.fn().mockImplementation((url: string) => {
      if (url.includes("/accept")) {
        return Promise.resolve(jsonResponse(200, { status: "accepted" }));
      }
      if (url.endsWith("/friends")) {
        return Promise.resolve(jsonResponse(200, []));
      }
      return Promise.resolve(jsonResponse(200, []));
    });
    vi.stubGlobal("fetch", fetchSpy);
    const { result } = renderHook(() => useFriends(), { wrapper: Wrapper });
    await waitFor(() => expect(result.current.loading).toBe(false));

    await act(async () => {
      await result.current.acceptRequest(1);
    });

    expect(fetchSpy).toHaveBeenCalledWith(
      expect.stringContaining("/friends/requests/1/accept"),
      expect.anything(),
    );
  });

  it("sendRequest failure surfaces as a rejection, not a silent no-op", async () => {
    vi.stubGlobal("fetch", vi.fn().mockResolvedValue(jsonResponse(400, { error: "cannot friend yourself" })));
    const { result } = renderHook(() => useFriends(), { wrapper: Wrapper });
    await waitFor(() => expect(result.current.loading).toBe(false));

    await expect(result.current.sendRequest("alice")).rejects.toThrow();
  });
});
