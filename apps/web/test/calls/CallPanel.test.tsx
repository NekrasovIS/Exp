import { ChatClient } from "@devicehub/core";
import { render, screen } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";

import { CallPanel } from "../../src/calls/CallPanel.js";
import { FakeWebSocket } from "../testUtils.js";

function fakeMediaStream(): MediaStream {
  const track = { kind: "audio", enabled: true, stop: vi.fn(), onended: null };
  return { getAudioTracks: () => [track], getVideoTracks: () => [] } as unknown as MediaStream;
}

beforeEach(() => {
  FakeWebSocket.instances = [];
  vi.stubGlobal("WebSocket", FakeWebSocket);
  // jsdom doesn't implement getUserMedia at all — CallManager's default
  // constructor param reaches for navigator.mediaDevices directly (see
  // its own doc comment on why it's not always injected), so this
  // stands in the same way FakeWebSocket stands in for the WebSocket
  // global above.
  Object.defineProperty(navigator, "mediaDevices", {
    configurable: true,
    value: { getUserMedia: vi.fn().mockResolvedValue(fakeMediaStream()) },
  });
});

afterEach(() => {
  vi.unstubAllGlobals();
});

describe("CallPanel", () => {
  it("shows a Join call button before joining", () => {
    const client = new ChatClient("wss://chat.example.test", (url) => new FakeWebSocket(url));
    client.connectToChannel("t1", 7);

    render(<CallPanel chatClient={client} localLogin="alice" />);

    expect(screen.getByRole("button", { name: "Join call" })).toBeInTheDocument();
  });

  it("sends call_join and switches to in-call controls when clicked", async () => {
    const client = new ChatClient("wss://chat.example.test", (url) => new FakeWebSocket(url));
    client.connectToChannel("t1", 7);
    const socket = FakeWebSocket.instances[0];

    render(<CallPanel chatClient={client} localLogin="alice" />);
    await userEvent.click(screen.getByRole("button", { name: "Join call" }));

    expect(socket?.sent.map((frame) => JSON.parse(frame))).toContainEqual({ call_join: true });
    expect(await screen.findByRole("button", { name: "Mute" })).toBeInTheDocument();
    expect(screen.getByRole("button", { name: "Enable video" })).toBeInTheDocument();
    expect(screen.getByRole("button", { name: "Share screen" })).toBeInTheDocument();
    expect(screen.getByRole("button", { name: "Leave call" })).toBeInTheDocument();
  });

  it("shows joined participants and lets you leave the call", async () => {
    const client = new ChatClient("wss://chat.example.test", (url) => new FakeWebSocket(url));
    client.connectToChannel("t1", 7);
    const socket = FakeWebSocket.instances[0];

    render(<CallPanel chatClient={client} localLogin="alice" />);
    await userEvent.click(screen.getByRole("button", { name: "Join call" }));
    socket?.onmessage?.({ data: JSON.stringify({ call_peer_joined: "bob" }) });

    expect(await screen.findByText(/In call: bob/)).toBeInTheDocument();

    await userEvent.click(screen.getByRole("button", { name: "Leave call" }));

    expect(socket?.sent.map((frame) => JSON.parse(frame))).toContainEqual({ call_leave: true });
    expect(screen.getByRole("button", { name: "Join call" })).toBeInTheDocument();
  });
});
