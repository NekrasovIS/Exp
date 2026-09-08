import { act, renderHook, waitFor } from "@testing-library/react";
import React from "react";
import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";

import { useDmThreads } from "../../src/dm/useDmThreads.js";
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

describe("useDmThreads", () => {
  it("loads existing threads", async () => {
    vi.stubGlobal(
      "fetch",
      vi
        .fn()
        .mockResolvedValue(
          jsonResponse(200, [{ id: 1, other_login: "bob", created_at: "2026-01-01T00:00:00Z" }]),
        ),
    );
    const { result } = renderHook(() => useDmThreads(), { wrapper: Wrapper });

    await waitFor(() => expect(result.current.loading).toBe(false));
    expect(result.current.threads).toEqual([{ id: 1, otherLogin: "bob", createdAt: "2026-01-01T00:00:00Z" }]);
  });

  it("openThreadWith opens (or reopens) a thread and returns its id", async () => {
    const fetchSpy = vi
      .fn()
      .mockResolvedValueOnce(jsonResponse(200, []))
      .mockResolvedValueOnce(jsonResponse(200, { id: 42 }))
      .mockResolvedValueOnce(
        jsonResponse(200, [{ id: 42, other_login: "bob", created_at: "2026-01-01T00:00:00Z" }]),
      );
    vi.stubGlobal("fetch", fetchSpy);
    const { result } = renderHook(() => useDmThreads(), { wrapper: Wrapper });
    await waitFor(() => expect(result.current.loading).toBe(false));

    let threadId: number | null = null;
    await act(async () => {
      threadId = await result.current.openThreadWith("bob");
    });

    expect(threadId).toBe(42);
    expect(result.current.threads).toEqual([
      { id: 42, otherLogin: "bob", createdAt: "2026-01-01T00:00:00Z" },
    ]);
  });
});
