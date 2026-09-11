import { act, renderHook, waitFor } from "@testing-library/react";
import React from "react";
import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";

import { useUnreadCounts } from "../../src/chat/useUnreadCounts.js";
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
  vi.useRealTimers();
  localStorage.clear();
});

describe("useUnreadCounts", () => {
  it("fetches and maps channel/thread counts on mount", async () => {
    vi.stubGlobal(
      "fetch",
      vi.fn().mockResolvedValue(
        jsonResponse(200, {
          channels: [{ channel_id: 10, unread_count: 3 }],
          dm_threads: [{ thread_id: 7, unread_count: 1 }],
        }),
      ),
    );
    const { result } = renderHook(() => useUnreadCounts(), { wrapper: Wrapper });

    await waitFor(() => expect(result.current.channelCounts.get(10)).toBe(3));
    expect(result.current.threadCounts.get(7)).toBe(1);
  });

  it("clearChannelLocally/clearThreadLocally zero an entry without a network call", async () => {
    const fetchSpy = vi.fn().mockResolvedValue(
      jsonResponse(200, {
        channels: [{ channel_id: 10, unread_count: 3 }],
        dm_threads: [{ thread_id: 7, unread_count: 1 }],
      }),
    );
    vi.stubGlobal("fetch", fetchSpy);
    const { result } = renderHook(() => useUnreadCounts(), { wrapper: Wrapper });
    await waitFor(() => expect(result.current.channelCounts.get(10)).toBe(3));
    const callsBeforeClear = fetchSpy.mock.calls.length;

    act(() => result.current.clearChannelLocally(10));
    act(() => result.current.clearThreadLocally(7));

    expect(result.current.channelCounts.get(10)).toBe(0);
    expect(result.current.threadCounts.get(7)).toBe(0);
    expect(fetchSpy).toHaveBeenCalledTimes(callsBeforeClear);
  });

  it("polls again after the 30s interval elapses", async () => {
    // Fake timers must be active before mount — the poll setInterval()
    // is registered in the same effect that fires the initial refresh(),
    // so arming them only afterward (as some other tests in this repo
    // do, once the *initial* async work has settled) would leave that
    // interval on the real clock, unaffected by advanceTimersByTimeAsync().
    vi.useFakeTimers();
    const fetchSpy = vi.fn().mockResolvedValue(jsonResponse(200, { channels: [], dm_threads: [] }));
    vi.stubGlobal("fetch", fetchSpy);
    renderHook(() => useUnreadCounts(), { wrapper: Wrapper });
    await act(async () => {
      await vi.advanceTimersByTimeAsync(0); // Flushes the initial refresh() call.
    });
    expect(fetchSpy).toHaveBeenCalledTimes(1);

    await act(async () => {
      await vi.advanceTimersByTimeAsync(30_000);
    });

    expect(fetchSpy.mock.calls.length).toBeGreaterThanOrEqual(2);
  });
});
