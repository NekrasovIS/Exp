import { describe, expect, it, vi } from "vitest";

import { ChatClient } from "../../src/chat/ChatClient.js";
import type { WebSocketFactory, WebSocketLike } from "../../src/chat/ChatClient.js";

class FakeWebSocket implements WebSocketLike {
  onopen: (() => void) | null = null;
  onclose: (() => void) | null = null;
  onerror: (() => void) | null = null;
  onmessage: ((event: { data: unknown }) => void) | null = null;
  readonly sent: string[] = [];
  closed = false;

  send(data: string): void {
    this.sent.push(data);
  }

  close(): void {
    this.closed = true;
    this.onclose?.();
  }

  simulateOpen(): void {
    this.onopen?.();
  }

  simulateMessage(data: unknown): void {
    this.onmessage?.({ data });
  }

  simulateError(): void {
    this.onerror?.();
  }

  lastSentFrame(): unknown {
    return JSON.parse(this.sent[this.sent.length - 1] ?? "null");
  }
}

/** @p autoConnect defaults to true — most tests here care about
 * dispatch/send behavior on an already-connected client, not the
 * handshake itself (that's covered separately below), so this keeps
 * every other test from having to call connectToChannel() just to get a
 * socket wired up to the client's own message handler. */
function makeClientAndSocket(autoConnect = true): { client: ChatClient; socket: FakeWebSocket } {
  const socket = new FakeWebSocket();
  const factory: WebSocketFactory = vi.fn().mockReturnValue(socket);
  const client = new ChatClient("wss://chat.example.test", factory);
  if (autoConnect) {
    client.connectToChannel("t1", 1);
  }
  return { client, socket };
}

describe("ChatClient", () => {
  describe("Hello handshake", () => {
    it("sends {token, channel_id} on open for a channel subscription", () => {
      const { client, socket } = makeClientAndSocket();

      client.connectToChannel("t1", 42);
      socket.simulateOpen();

      expect(socket.lastSentFrame()).toEqual({ token: "t1", channel_id: 42 });
    });

    it("sends {token, dm_thread_id} on open for a DM-thread subscription", () => {
      const { client, socket } = makeClientAndSocket();

      client.connectToDirectMessageThread("t1", 7);
      socket.simulateOpen();

      expect(socket.lastSentFrame()).toEqual({ token: "t1", dm_thread_id: 7 });
    });
  });

  describe("subscribed", () => {
    it("emits the channel_id when subscribing to a channel", () => {
      const { client, socket } = makeClientAndSocket();
      const onSubscribed = vi.fn();
      client.on("subscribed", onSubscribed);
      client.connectToChannel("t1", 42);

      socket.simulateMessage(JSON.stringify({ subscribed: true, channel_id: 42 }));

      expect(onSubscribed).toHaveBeenCalledWith(42);
    });

    it("emits the dm_thread_id when subscribing to a DM thread", () => {
      const { client, socket } = makeClientAndSocket();
      const onSubscribed = vi.fn();
      client.on("subscribed", onSubscribed);
      client.connectToDirectMessageThread("t1", 7);

      socket.simulateMessage(JSON.stringify({ subscribed: true, dm_thread_id: 7 }));

      expect(onSubscribed).toHaveBeenCalledWith(7);
    });
  });

  it("emits 'message' for an incoming chat message, with a null attachment_id becoming undefined", () => {
    const { client, socket } = makeClientAndSocket();
    const onMessage = vi.fn();
    client.on("message", onMessage);

    socket.simulateMessage(
      JSON.stringify({
        id: 1,
        author: "alice",
        body: "hi",
        sent_at: "2026-01-01T00:00:00Z",
        attachment_id: null,
      }),
    );

    expect(onMessage).toHaveBeenCalledWith({
      id: 1,
      author: "alice",
      body: "hi",
      sentAt: "2026-01-01T00:00:00Z",
    });
  });

  it("emits 'messageEdited'", () => {
    const { client, socket } = makeClientAndSocket();
    const onEdited = vi.fn();
    client.on("messageEdited", onEdited);

    socket.simulateMessage(JSON.stringify({ message_edited: { id: 1, body: "edited", edited_at: "now" } }));

    expect(onEdited).toHaveBeenCalledWith(1, "edited", "now");
  });

  it("emits 'messageDeleted'", () => {
    const { client, socket } = makeClientAndSocket();
    const onDeleted = vi.fn();
    client.on("messageDeleted", onDeleted);

    socket.simulateMessage(JSON.stringify({ message_deleted: { id: 1 } }));

    expect(onDeleted).toHaveBeenCalledWith(1);
  });

  it("emits 'error' for a protocol-level {error} frame", () => {
    const { client, socket } = makeClientAndSocket();
    const onError = vi.fn();
    client.on("error", onError);

    socket.simulateMessage(JSON.stringify({ error: "not subscribed" }));

    expect(onError).toHaveBeenCalledWith("not subscribed");
  });

  it("emits 'error' with a fixed message for malformed (non-object) JSON", () => {
    const { client, socket } = makeClientAndSocket();
    const onError = vi.fn();
    client.on("error", onError);

    socket.simulateMessage("not json at all");

    expect(onError).toHaveBeenCalledWith("Malformed message from chat-service");
  });

  it("emits 'error' on a transport-level error", () => {
    const { client, socket } = makeClientAndSocket();
    const onError = vi.fn();
    client.on("error", onError);

    socket.simulateError();

    expect(onError).toHaveBeenCalledWith("WebSocket transport error");
  });

  describe("call signaling", () => {
    it("emits 'callRoster'", () => {
      const { client, socket } = makeClientAndSocket();
      const onRoster = vi.fn();
      client.on("callRoster", onRoster);

      socket.simulateMessage(JSON.stringify({ call_roster: ["bob", "carol"] }));

      expect(onRoster).toHaveBeenCalledWith(["bob", "carol"]);
    });

    it("emits 'callPeerJoined' and 'callPeerLeft'", () => {
      const { client, socket } = makeClientAndSocket();
      const onJoined = vi.fn();
      const onLeft = vi.fn();
      client.on("callPeerJoined", onJoined);
      client.on("callPeerLeft", onLeft);

      socket.simulateMessage(JSON.stringify({ call_peer_joined: "bob" }));
      socket.simulateMessage(JSON.stringify({ call_peer_left: "bob" }));

      expect(onJoined).toHaveBeenCalledWith("bob");
      expect(onLeft).toHaveBeenCalledWith("bob");
    });

    it("emits 'callSignal' with the opaque payload untouched", () => {
      const { client, socket } = makeClientAndSocket();
      const onSignal = vi.fn();
      client.on("callSignal", onSignal);
      const payload = { sdp: "opaque-offer-data" };

      socket.simulateMessage(JSON.stringify({ call_signal: { from: "bob", payload } }));

      expect(onSignal).toHaveBeenCalledWith("bob", payload);
    });

    it("emits 'callReaction' with the login and emoji", () => {
      const { client, socket } = makeClientAndSocket();
      const onReaction = vi.fn();
      client.on("callReaction", onReaction);

      socket.simulateMessage(JSON.stringify({ call_reaction: { login: "bob", emoji: "👍" } }));

      expect(onReaction).toHaveBeenCalledWith("bob", "👍");
    });

    it("emits 'sfuRoomAssigned' alongside 'callRoster' when sfu_room is present", () => {
      const { client, socket } = makeClientAndSocket();
      const onRoster = vi.fn();
      const onSfuRoom = vi.fn();
      client.on("callRoster", onRoster);
      client.on("sfuRoomAssigned", onSfuRoom);

      socket.simulateMessage(JSON.stringify({ call_roster: ["bob"], sfu_room: "room-42" }));

      expect(onRoster).toHaveBeenCalledWith(["bob"]);
      expect(onSfuRoom).toHaveBeenCalledWith("room-42");
    });

    it("does not emit 'sfuRoomAssigned' when sfu_room is absent (Janus unavailable)", () => {
      const { client, socket } = makeClientAndSocket();
      const onSfuRoom = vi.fn();
      client.on("sfuRoomAssigned", onSfuRoom);

      socket.simulateMessage(JSON.stringify({ call_roster: ["bob"] }));

      expect(onSfuRoom).not.toHaveBeenCalled();
    });
  });

  describe("SFU signaling proxy", () => {
    it("emits 'janusAttached' with the new handle", () => {
      const { client, socket } = makeClientAndSocket();
      const onAttached = vi.fn();
      client.on("janusAttached", onAttached);

      socket.simulateMessage(JSON.stringify({ janus_attached: { handle: 7 } }));

      expect(onAttached).toHaveBeenCalledWith(7);
    });

    it("emits 'janusMessageAck' with the response object", () => {
      const { client, socket } = makeClientAndSocket();
      const onAck = vi.fn();
      client.on("janusMessageAck", onAck);

      socket.simulateMessage(JSON.stringify({ janus_message_ack: { janus: "ack" } }));

      expect(onAck).toHaveBeenCalledWith({ janus: "ack" });
    });

    it("emits 'janusEvent' with the event object untouched", () => {
      const { client, socket } = makeClientAndSocket();
      const onEvent = vi.fn();
      client.on("janusEvent", onEvent);
      const event = { sender: 7, plugindata: { data: { videoroom: "joined" } } };

      socket.simulateMessage(JSON.stringify({ janus_event: event }));

      expect(onEvent).toHaveBeenCalledWith(event);
    });

    it("sendJanusAttach sends the expected frame", () => {
      const { client, socket } = makeClientAndSocket();
      client.connectToChannel("t1", 1);

      client.sendJanusAttach();

      expect(socket.lastSentFrame()).toEqual({ janus_attach: true });
    });

    it("sendJanusMessage without jsep", () => {
      const { client, socket } = makeClientAndSocket();
      client.connectToChannel("t1", 1);

      client.sendJanusMessage(7, { request: "join", room: "room-42" });

      expect(socket.lastSentFrame()).toEqual({
        janus_message: { handle: 7, body: { request: "join", room: "room-42" } },
      });
    });

    it("sendJanusMessage with jsep includes it", () => {
      const { client, socket } = makeClientAndSocket();
      client.connectToChannel("t1", 1);
      const jsep = { type: "offer", sdp: "opaque-sdp" };

      client.sendJanusMessage(7, { request: "configure" }, jsep);

      expect(socket.lastSentFrame()).toEqual({
        janus_message: { handle: 7, body: { request: "configure" }, jsep },
      });
    });
  });

  it("emits 'userTyping'", () => {
    const { client, socket } = makeClientAndSocket();
    const onTyping = vi.fn();
    client.on("userTyping", onTyping);

    socket.simulateMessage(JSON.stringify({ user_typing: "bob" }));

    expect(onTyping).toHaveBeenCalledWith("bob");
  });

  it("silently drops a frame matching none of the known shapes", () => {
    const { client, socket } = makeClientAndSocket();
    const anyListener = vi.fn();
    for (const event of ["subscribed", "message", "error", "userTyping"] as const) {
      client.on(event, anyListener);
    }

    socket.simulateMessage(JSON.stringify({ something_unrelated: true }));

    expect(anyListener).not.toHaveBeenCalled();
  });

  describe("outgoing frames", () => {
    it("sendMessage without an attachment", () => {
      const { client, socket } = makeClientAndSocket();
      client.connectToChannel("t1", 1);

      client.sendMessage("hello");

      expect(socket.lastSentFrame()).toEqual({ body: "hello" });
    });

    it("sendMessage with an attachment includes attachment_id", () => {
      const { client, socket } = makeClientAndSocket();
      client.connectToChannel("t1", 1);

      client.sendMessage("see attached", 5);

      expect(socket.lastSentFrame()).toEqual({ body: "see attached", attachment_id: 5 });
    });

    it("joinCall/leaveCall send the expected frames", () => {
      const { client, socket } = makeClientAndSocket();
      client.connectToChannel("t1", 1);

      client.joinCall();
      expect(socket.lastSentFrame()).toEqual({ call_join: true });

      client.leaveCall();
      expect(socket.lastSentFrame()).toEqual({ call_leave: true });
    });

    it("sendCallSignal relays the opaque payload", () => {
      const { client, socket } = makeClientAndSocket();
      client.connectToChannel("t1", 1);
      const payload = { candidate: "opaque" };

      client.sendCallSignal("bob", payload);

      expect(socket.lastSentFrame()).toEqual({ call_signal: { to: "bob", payload } });
    });

    it("sendCallReaction sends the emoji as a bare string", () => {
      const { client, socket } = makeClientAndSocket();
      client.connectToChannel("t1", 1);

      client.sendCallReaction("👍");

      expect(socket.lastSentFrame()).toEqual({ call_reaction: "👍" });
    });

    it("sendTyping/sendEditMessage/sendDeleteMessage send the expected frames", () => {
      const { client, socket } = makeClientAndSocket();
      client.connectToChannel("t1", 1);

      client.sendTyping();
      expect(socket.lastSentFrame()).toEqual({ typing: true });

      client.sendEditMessage(1, "new body");
      expect(socket.lastSentFrame()).toEqual({ edit_message: { id: 1, body: "new body" } });

      client.sendDeleteMessage(1);
      expect(socket.lastSentFrame()).toEqual({ delete_message: { id: 1 } });
    });

    it("throws when sending before a connection is established", () => {
      const { client } = makeClientAndSocket(/* autoConnect= */ false);

      expect(() => client.sendMessage("too early")).toThrow(/not connected/);
    });
  });

  it("disconnect() closes the underlying socket", () => {
    const { client, socket } = makeClientAndSocket();
    client.connectToChannel("t1", 1);

    client.disconnect();

    expect(socket.closed).toBe(true);
  });

  it("on() returns an unsubscribe function that stops future calls", () => {
    const { client, socket } = makeClientAndSocket();
    const onTyping = vi.fn();
    const unsubscribe = client.on("userTyping", onTyping);

    unsubscribe();
    socket.simulateMessage(JSON.stringify({ user_typing: "bob" }));

    expect(onTyping).not.toHaveBeenCalled();
  });
});
