import { act, renderHook } from "@testing-library/react";
import React from "react";
import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";

import { useChatSocket } from "../../src/chat/useChatSocket.js";
import { SessionProvider } from "../../src/session/SessionContext.js";

const kStorageKey = "devicehub.web.session";

// jsdom doesn't implement WebSocket at all — useChatSocket's default
// factory (`(url) => new WebSocket(url)`) needs *some* global to
// construct, so this fake stands in for it. Only the surface ChatClient
// actually touches (WebSocketLike) is implemented.
class FakeWebSocket {
  static instances: FakeWebSocket[] = [];
  closed = false;
  onopen: (() => void) | null = null;
  onclose: (() => void) | null = null;
  onerror: (() => void) | null = null;
  onmessage: ((event: { data: unknown }) => void) | null = null;

  constructor(public readonly url: string) {
    FakeWebSocket.instances.push(this);
  }

  send(_data: string): void {}

  close(): void {
    this.closed = true;
  }
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

function Wrapper({ children }: { children: React.ReactNode }) {
  return <SessionProvider>{children}</SessionProvider>;
}

describe("useChatSocket", () => {
  it("connects to the given channel while mounted and disconnects on unmount", () => {
    const { unmount } = renderHook(() => useChatSocket({ channelId: 42 }), { wrapper: Wrapper });

    expect(FakeWebSocket.instances).toHaveLength(1);
    const socket = FakeWebSocket.instances[0]!;
    expect(socket.closed).toBe(false);

    act(() => socket.onopen?.());

    unmount();
    expect(socket.closed).toBe(true);
  });

  it("reconnects when the target channel changes", () => {
    const { rerender } = renderHook(({ channelId }) => useChatSocket({ channelId }), {
      wrapper: Wrapper,
      initialProps: { channelId: 1 },
    });
    expect(FakeWebSocket.instances).toHaveLength(1);
    const firstSocket = FakeWebSocket.instances[0]!;

    rerender({ channelId: 2 });
    expect(firstSocket.closed).toBe(true);
    expect(FakeWebSocket.instances).toHaveLength(2);
  });
});
