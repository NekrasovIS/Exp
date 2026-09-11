import { act, renderHook, waitFor } from "@testing-library/react";
import React from "react";
import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";

import { useChatSocket } from "../../src/chat/useChatSocket.js";
import { usePinnedMessages } from "../../src/chat/usePinnedMessages.js";
import { SessionProvider } from "../../src/session/SessionContext.js";
import { FakeWebSocket, jsonResponse } from "../testUtils.js";

const kStorageKey = "devicehub.web.session";

function Wrapper({ children }: { children: React.ReactNode }) {
  return <SessionProvider>{children}</SessionProvider>;
}

function usePinnedWithSocket(channelId: number) {
  const socket = useChatSocket({ channelId });
  return usePinnedMessages(channelId, socket);
}

beforeEach(() => {
  localStorage.clear();
  FakeWebSocket.instances = [];
  vi.stubGlobal("WebSocket", FakeWebSocket);
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
});

describe("usePinnedMessages", () => {
  it("loads the pinned list on mount", async () => {
    vi.stubGlobal(
      "fetch",
      vi.fn().mockResolvedValue(
        jsonResponse(200, [
          {
            id: 1,
            author: "alice",
            body: "pin me",
            sent_at: "2026-01-01T00:00:00Z",
            pinned_by: "bob",
            pinned_at: "2026-01-01T00:05:00Z",
          },
        ]),
      ),
    );

    const { result } = renderHook(() => usePinnedWithSocket(7), { wrapper: Wrapper });

    await waitFor(() => expect(result.current.pinned).toHaveLength(1));
    expect(result.current.pinnedIds.has(1)).toBe(true);
  });

  it("refetches the list on a live messagePinned event", async () => {
    const fetchSpy = vi
      .fn()
      .mockResolvedValueOnce(jsonResponse(200, []))
      .mockResolvedValueOnce(
        jsonResponse(200, [
          {
            id: 2,
            author: "alice",
            body: "newly pinned",
            sent_at: "2026-01-01T00:00:00Z",
            pinned_by: "bob",
            pinned_at: "2026-01-01T00:05:00Z",
          },
        ]),
      );
    vi.stubGlobal("fetch", fetchSpy);
    const { result } = renderHook(() => usePinnedWithSocket(7), { wrapper: Wrapper });
    await waitFor(() => expect(fetchSpy).toHaveBeenCalledTimes(1));

    const socket = FakeWebSocket.instances[0]!;
    act(() => {
      socket.onmessage?.({
        data: JSON.stringify({
          message_pinned: { id: 2, pinned_by: "bob", pinned_at: "2026-01-01T00:05:00Z" },
        }),
      });
    });

    await waitFor(() => expect(result.current.pinnedIds.has(2)).toBe(true));
  });

  it("removes the message locally on a live messageUnpinned event, without refetching", async () => {
    const fetchSpy = vi.fn().mockResolvedValue(
      jsonResponse(200, [
        {
          id: 1,
          author: "alice",
          body: "pin me",
          sent_at: "2026-01-01T00:00:00Z",
          pinned_by: "bob",
          pinned_at: "2026-01-01T00:05:00Z",
        },
      ]),
    );
    vi.stubGlobal("fetch", fetchSpy);
    const { result } = renderHook(() => usePinnedWithSocket(7), { wrapper: Wrapper });
    await waitFor(() => expect(result.current.pinned).toHaveLength(1));
    const callsBeforeUnpin = fetchSpy.mock.calls.length;

    const socket = FakeWebSocket.instances[0]!;
    act(() => {
      socket.onmessage?.({ data: JSON.stringify({ message_unpinned: { id: 1 } }) });
    });

    expect(result.current.pinned).toHaveLength(0);
    expect(fetchSpy.mock.calls.length).toBe(callsBeforeUnpin);
  });

  it("pin()/unpin() send the expected frames over the socket", async () => {
    vi.stubGlobal("fetch", vi.fn().mockResolvedValue(jsonResponse(200, [])));
    const { result } = renderHook(() => usePinnedWithSocket(7), { wrapper: Wrapper });
    await waitFor(() => expect(FakeWebSocket.instances).toHaveLength(1));
    const socket = FakeWebSocket.instances[0]!;

    act(() => result.current.pin(1));
    act(() => result.current.unpin(1));

    expect(socket.sent.map((frame) => JSON.parse(frame))).toEqual([
      { pin_message: { id: 1 } },
      { unpin_message: { id: 1 } },
    ]);
  });
});
