import { act, renderHook, waitFor } from "@testing-library/react";
import React from "react";
import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";

import { useChatSocket } from "../../src/chat/useChatSocket.js";
import { useReadReceipts } from "../../src/chat/useReadReceipts.js";
import { SessionProvider } from "../../src/session/SessionContext.js";
import { FakeWebSocket, jsonResponse } from "../testUtils.js";

const kStorageKey = "devicehub.web.session";

function Wrapper({ children }: { children: React.ReactNode }) {
  return <SessionProvider>{children}</SessionProvider>;
}

function useReadReceiptsWithSocket(channelId: number) {
  const socket = useChatSocket({ channelId });
  return useReadReceipts({ channelId }, socket);
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

describe("useReadReceipts", () => {
  it("loads the initial snapshot on mount", async () => {
    vi.stubGlobal(
      "fetch",
      vi.fn().mockResolvedValue(jsonResponse(200, { receipts: [{ login: "bob", last_read_message_id: 5 }] })),
    );

    const { result } = renderHook(() => useReadReceiptsWithSocket(7), { wrapper: Wrapper });

    await waitFor(() => expect(result.current.readPointers.get("bob")).toBe(5));
  });

  it("advances a pointer on a live readReceiptChanged event", async () => {
    vi.stubGlobal("fetch", vi.fn().mockResolvedValue(jsonResponse(200, { receipts: [] })));
    const { result } = renderHook(() => useReadReceiptsWithSocket(7), { wrapper: Wrapper });
    await waitFor(() => expect(FakeWebSocket.instances).toHaveLength(1));

    const socket = FakeWebSocket.instances[0]!;
    act(() => {
      socket.onmessage?.({
        data: JSON.stringify({ read_receipt: { login: "bob", last_read_message_id: 5 } }),
      });
    });

    await waitFor(() => expect(result.current.readPointers.get("bob")).toBe(5));
  });

  it("never moves a pointer backward on a stale/out-of-order event", async () => {
    vi.stubGlobal(
      "fetch",
      vi.fn().mockResolvedValue(jsonResponse(200, { receipts: [{ login: "bob", last_read_message_id: 5 }] })),
    );
    const { result } = renderHook(() => useReadReceiptsWithSocket(7), { wrapper: Wrapper });
    await waitFor(() => expect(result.current.readPointers.get("bob")).toBe(5));

    const socket = FakeWebSocket.instances[0]!;
    act(() => {
      socket.onmessage?.({
        data: JSON.stringify({ read_receipt: { login: "bob", last_read_message_id: 2 } }),
      });
    });

    expect(result.current.readPointers.get("bob")).toBe(5);
  });
});
