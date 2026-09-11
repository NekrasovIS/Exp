import { act, renderHook, waitFor } from "@testing-library/react";
import React from "react";
import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";

import { useDirectMessages } from "../../src/dm/useDirectMessages.js";
import { SessionProvider } from "../../src/session/SessionContext.js";
import { FakeWebSocket, jsonResponse } from "../testUtils.js";

const kStorageKey = "devicehub.web.session";

function Wrapper({ children }: { children: React.ReactNode }) {
  return <SessionProvider>{children}</SessionProvider>;
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

describe("useDirectMessages", () => {
  it("loads the initial history and subscribes to the thread (not a channel)", async () => {
    vi.stubGlobal(
      "fetch",
      vi
        .fn()
        .mockResolvedValue(
          jsonResponse(200, [{ id: 1, author: "alice", body: "hi", sent_at: "2026-01-01T00:00:00Z" }]),
        ),
    );
    const { result } = renderHook(() => useDirectMessages(9), { wrapper: Wrapper });

    await waitFor(() => expect(result.current.loading).toBe(false));
    expect(result.current.messages).toEqual([
      { id: 1, author: "alice", body: "hi", sentAt: "2026-01-01T00:00:00Z" },
    ]);

    const socket = FakeWebSocket.instances[0]!;
    socket.onopen?.();
    const hello = JSON.parse(socket.sent[0]!);
    expect(hello).toEqual({ token: "access-token", dm_thread_id: 9 });
  });

  it("appends a live message received over the socket", async () => {
    vi.stubGlobal("fetch", vi.fn().mockResolvedValue(jsonResponse(200, [])));
    const { result } = renderHook(() => useDirectMessages(9), { wrapper: Wrapper });
    await waitFor(() => expect(result.current.loading).toBe(false));

    const socket = FakeWebSocket.instances[0]!;
    act(() => {
      socket.onmessage?.({
        data: JSON.stringify({ id: 2, author: "bob", body: "hello", sent_at: "2026-01-01T00:01:00Z" }),
      });
    });

    expect(result.current.messages).toHaveLength(1);
    expect(result.current.messages[0]?.body).toBe("hello");
  });

  it("sendMessage sends a plain {body} frame over the socket (no attachment_id field)", async () => {
    vi.stubGlobal("fetch", vi.fn().mockResolvedValue(jsonResponse(200, [])));
    const { result } = renderHook(() => useDirectMessages(9), { wrapper: Wrapper });
    await waitFor(() => expect(result.current.loading).toBe(false));
    const socket = FakeWebSocket.instances[0]!;

    act(() => result.current.sendMessage("hello there"));

    expect(socket.sent.map((frame) => JSON.parse(frame))).toEqual([{ body: "hello there" }]);
  });

  // Issue #313 — DM threads now support the same {"typing"}/"user_typing"
  // protocol as channels (chat-service's handleDirectMessage()). Fake
  // timers are switched on only after the initial async load already
  // settled under real timers — mixing them earlier makes
  // @testing-library's own waitFor polling unreliable.
  describe("typing", () => {
    afterEach(() => {
      vi.useRealTimers();
    });

    it("sendTyping sends a {typing:true} frame, throttled while called repeatedly", async () => {
      vi.stubGlobal("fetch", vi.fn().mockResolvedValue(jsonResponse(200, [])));
      const { result } = renderHook(() => useDirectMessages(9), { wrapper: Wrapper });
      await waitFor(() => expect(result.current.loading).toBe(false));
      const socket = FakeWebSocket.instances[0]!;

      vi.useFakeTimers();
      act(() => {
        result.current.sendTyping();
        result.current.sendTyping();
      });
      expect(socket.sent.map((frame) => JSON.parse(frame))).toEqual([{ typing: true }]);

      // Throttle window elapses — the next call sends a fresh frame again.
      act(() => vi.advanceTimersByTime(2000));
      act(() => result.current.sendTyping());
      expect(socket.sent.map((frame) => JSON.parse(frame))).toEqual([{ typing: true }, { typing: true }]);
    });

    it("a received user_typing sets typingUser, then auto-clears after the hide window", async () => {
      vi.stubGlobal("fetch", vi.fn().mockResolvedValue(jsonResponse(200, [])));
      const { result } = renderHook(() => useDirectMessages(9), { wrapper: Wrapper });
      await waitFor(() => expect(result.current.loading).toBe(false));
      const socket = FakeWebSocket.instances[0]!;

      vi.useFakeTimers();
      act(() => socket.onmessage?.({ data: JSON.stringify({ user_typing: "bob" }) }));
      expect(result.current.typingUser).toBe("bob");

      act(() => vi.advanceTimersByTime(3000));
      expect(result.current.typingUser).toBeNull();
    });
  });
});
