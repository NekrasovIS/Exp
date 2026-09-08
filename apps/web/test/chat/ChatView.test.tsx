import { render, screen } from "@testing-library/react";
import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";

import { ChatView } from "../../src/chat/ChatView.js";
import { SessionProvider } from "../../src/session/SessionContext.js";
import { FakeWebSocket, jsonResponse } from "../testUtils.js";

const kStorageKey = "devicehub.web.session";

beforeEach(() => {
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
  localStorage.clear();
});

describe("ChatView", () => {
  it("shows a placeholder for an encrypted channel and never opens a socket or fetches", () => {
    const fetchSpy = vi.fn();
    vi.stubGlobal("fetch", fetchSpy);

    render(
      <SessionProvider>
        <ChatView channelId={7} communityId={1} isEncrypted={true} />
      </SessionProvider>,
    );

    expect(screen.getByText(/aren't supported in the web client yet/i)).toBeInTheDocument();
    expect(fetchSpy).not.toHaveBeenCalled();
    expect(FakeWebSocket.instances).toHaveLength(0);
  });

  it("mounts the real chat content for a non-encrypted channel", async () => {
    vi.stubGlobal("fetch", vi.fn().mockResolvedValue(jsonResponse(200, [])));

    render(
      <SessionProvider>
        <ChatView channelId={7} communityId={1} isEncrypted={false} />
      </SessionProvider>,
    );

    expect(await screen.findByRole("button", { name: "Send" })).toBeInTheDocument();
    expect(FakeWebSocket.instances).toHaveLength(1);
  });
});
