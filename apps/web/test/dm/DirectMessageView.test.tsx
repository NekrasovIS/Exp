import { act, render, screen } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";

import { DirectMessageView } from "../../src/dm/DirectMessageView.js";
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

describe("DirectMessageView", () => {
  it("renders history and sends a new message over the socket", async () => {
    vi.stubGlobal(
      "fetch",
      vi
        .fn()
        .mockResolvedValue(
          jsonResponse(200, [{ id: 1, author: "alice", body: "hey", sent_at: "2026-01-01T00:00:00Z" }]),
        ),
    );

    render(
      <SessionProvider>
        <DirectMessageView threadId={9} otherLogin="bob" />
      </SessionProvider>,
    );

    expect(screen.getByRole("heading", { name: "bob" })).toBeInTheDocument();
    expect(await screen.findByText("hey")).toBeInTheDocument();

    await userEvent.type(screen.getByLabelText("Message"), "hi there");
    await userEvent.click(screen.getByRole("button", { name: "Send" }));

    const socket = FakeWebSocket.instances[0]!;
    const sentFrames = socket.sent.map((frame) => JSON.parse(frame));
    expect(sentFrames).toContainEqual({ body: "hi there" });
  });

  it("typing in the composer sends a typing frame, and a received user_typing shows the indicator (issue #313)", async () => {
    vi.stubGlobal("fetch", vi.fn().mockResolvedValue(jsonResponse(200, [])));

    render(
      <SessionProvider>
        <DirectMessageView threadId={9} otherLogin="bob" />
      </SessionProvider>,
    );
    await screen.findByLabelText("Message");
    const socket = FakeWebSocket.instances[0]!;

    await userEvent.type(screen.getByLabelText("Message"), "h");
    expect(socket.sent.map((frame) => JSON.parse(frame))).toContainEqual({ typing: true });

    expect(screen.queryByText("bob is typing…")).not.toBeInTheDocument();
    act(() => socket.onmessage?.({ data: JSON.stringify({ user_typing: "bob" }) }));
    expect(screen.getByText("bob is typing…")).toBeInTheDocument();
  });
});
