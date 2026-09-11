import { ChatClient } from "@devicehub/core";
import { act, render, screen } from "@testing-library/react";
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

  it("clicking a reaction button sends call_reaction with that emoji", async () => {
    const client = new ChatClient("wss://chat.example.test", (url) => new FakeWebSocket(url));
    client.connectToChannel("t1", 7);
    const socket = FakeWebSocket.instances[0];

    render(<CallPanel chatClient={client} localLogin="alice" />);
    await userEvent.click(screen.getByRole("button", { name: "Join call" }));
    await userEvent.click(await screen.findByRole("button", { name: "👍" }));

    expect(socket?.sent.map((frame) => JSON.parse(frame))).toContainEqual({ call_reaction: "👍" });
  });

  it("shows another participant's reaction, then auto-hides it after 2.5s", async () => {
    const client = new ChatClient("wss://chat.example.test", (url) => new FakeWebSocket(url));
    client.connectToChannel("t1", 7);
    const socket = FakeWebSocket.instances[0];

    render(<CallPanel chatClient={client} localLogin="alice" />);
    // Real timers for the join itself (its promise chain needs to
    // actually resolve) — fake timers only start once we're already
    // in the call, so the reaction's own setTimeout is the only one
    // vi.advanceTimersByTime() below needs to account for.
    await userEvent.click(screen.getByRole("button", { name: "Join call" }));
    await screen.findByRole("button", { name: "Mute" });

    vi.useFakeTimers();
    try {
      act(() => {
        socket?.onmessage?.({ data: JSON.stringify({ call_reaction: { login: "bob", emoji: "🎉" } }) });
      });
      expect(screen.getByText("bob 🎉")).toBeInTheDocument();

      act(() => {
        vi.advanceTimersByTime(2500);
      });
      expect(screen.queryByText("bob 🎉")).not.toBeInTheDocument();
    } finally {
      vi.useRealTimers();
    }
  });
});
