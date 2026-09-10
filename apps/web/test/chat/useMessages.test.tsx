import { act, renderHook, waitFor } from "@testing-library/react";
import React from "react";
import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";

import { useMessages } from "../../src/chat/useMessages.js";
import { SessionProvider } from "../../src/session/SessionContext.js";
import { FakeWebSocket, jsonResponse, routedFetch } from "../testUtils.js";

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

describe("useMessages", () => {
  it("loads the initial page of history", async () => {
    vi.stubGlobal(
      "fetch",
      vi
        .fn()
        .mockResolvedValue(
          jsonResponse(200, [{ id: 1, author: "alice", body: "hi", sent_at: "2026-01-01T00:00:00Z" }]),
        ),
    );
    const { result } = renderHook(() => useMessages(7), { wrapper: Wrapper });

    await waitFor(() => expect(result.current.loading).toBe(false));
    expect(result.current.messages).toEqual([
      { id: 1, author: "alice", body: "hi", sentAt: "2026-01-01T00:00:00Z" },
    ]);
  });

  it("appends a live message received over the socket", async () => {
    vi.stubGlobal("fetch", vi.fn().mockResolvedValue(jsonResponse(200, [])));
    const { result } = renderHook(() => useMessages(7), { wrapper: Wrapper });
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

  it("updates a message and marks it edited on a live messageEdited event", async () => {
    vi.stubGlobal(
      "fetch",
      vi
        .fn()
        .mockResolvedValue(
          jsonResponse(200, [{ id: 1, author: "alice", body: "original", sent_at: "2026-01-01T00:00:00Z" }]),
        ),
    );
    const { result } = renderHook(() => useMessages(7), { wrapper: Wrapper });
    await waitFor(() => expect(result.current.loading).toBe(false));

    const socket = FakeWebSocket.instances[0]!;
    act(() => {
      socket.onmessage?.({
        data: JSON.stringify({
          message_edited: { id: 1, body: "edited text", edited_at: "2026-01-01T00:02:00Z" },
        }),
      });
    });

    expect(result.current.messages[0]?.body).toBe("edited text");
    expect(result.current.editedIds.has(1)).toBe(true);
  });

  it("removes a message on a live messageDeleted event", async () => {
    vi.stubGlobal(
      "fetch",
      vi
        .fn()
        .mockResolvedValue(
          jsonResponse(200, [{ id: 1, author: "alice", body: "gone soon", sent_at: "2026-01-01T00:00:00Z" }]),
        ),
    );
    const { result } = renderHook(() => useMessages(7), { wrapper: Wrapper });
    await waitFor(() => expect(result.current.loading).toBe(false));

    const socket = FakeWebSocket.instances[0]!;
    act(() => {
      socket.onmessage?.({ data: JSON.stringify({ message_deleted: { id: 1 } }) });
    });

    expect(result.current.messages).toHaveLength(0);
  });

  it("loadOlder prepends an older page using the oldest loaded message as before_id", async () => {
    // A full 50-item page is what tells the hook "there might be more"
    // (hasMore = page.length === pageSize) — a short first page (as a
    // real last-page-of-history would be) correctly makes loadOlder()
    // a no-op, so this test's first page must be full-sized to
    // exercise the pagination path at all.
    const firstPage = Array.from({ length: 50 }, (_, i) => ({
      id: i + 51,
      author: "alice",
      body: `message ${i + 51}`,
      sent_at: "2026-01-01T00:00:00Z",
    }));
    // Routed by URL, not call order (issue #310/#350 added a fire-and-
    // forget markChannelRead() call right after the initial page loads
    // — a plain call-order mock would silently start answering
    // loadOlder()'s request with whatever leftover response that extra
    // call didn't consume).
    const fetchSpy = routedFetch([
      [
        /before_id=51/,
        () =>
          jsonResponse(200, [{ id: 50, author: "alice", body: "fifty", sent_at: "2026-01-01T00:00:00Z" }]),
      ],
      [/\/channels\/7\/messages/, () => jsonResponse(200, firstPage)],
      [/\/channels\/7\/read/, () => jsonResponse(200, {})],
    ]);
    vi.stubGlobal("fetch", fetchSpy);
    const { result } = renderHook(() => useMessages(7), { wrapper: Wrapper });
    await waitFor(() => expect(result.current.loading).toBe(false));
    expect(result.current.hasMore).toBe(true);

    await act(async () => {
      await result.current.loadOlder();
    });

    expect(result.current.messages[0]?.id).toBe(50);
    expect(result.current.messages).toHaveLength(51);
  });

  it("sendMessage/editMessage/deleteMessage send the expected frames over the socket", async () => {
    vi.stubGlobal("fetch", vi.fn().mockResolvedValue(jsonResponse(200, [])));
    const { result } = renderHook(() => useMessages(7), { wrapper: Wrapper });
    await waitFor(() => expect(result.current.loading).toBe(false));
    const socket = FakeWebSocket.instances[0]!;

    act(() => result.current.sendMessage("hello", 9));
    act(() => result.current.editMessage(1, "edited"));
    act(() => result.current.deleteMessage(1));

    expect(socket.sent.map((frame) => JSON.parse(frame))).toEqual([
      { body: "hello", attachment_id: 9 },
      { edit_message: { id: 1, body: "edited" } },
      { delete_message: { id: 1 } },
    ]);
  });
});
