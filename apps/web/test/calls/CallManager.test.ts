import { ChatClient } from "@devicehub/core";
import type { WebSocketFactory, WebSocketLike } from "@devicehub/core";
import { describe, expect, it, vi } from "vitest";

import { CallManager, kIceGatheringTimeoutMs } from "../../src/calls/CallManager.js";
import type { RTCPeerConnectionLike } from "../../src/calls/CallManager.js";

/** Several negotiatePublish()/answerSubscribe() steps below run
 * fire-and-forget (onJanusEvent is a synchronous event handler, so it
 * can't itself await them) — each is a chain of several `await`s
 * (createOffer, setLocalDescription, ICE-gathering-complete) before the
 * resulting Janus message actually gets sent. Flushing several
 * microtask ticks is simpler and more robust than counting exactly how
 * many hops any given chain needs. */
async function flushAsync(): Promise<void> {
  for (let i = 0; i < 10; i += 1) {
    await Promise.resolve();
  }
}

class FakeWebSocket implements WebSocketLike {
  onopen: (() => void) | null = null;
  onclose: (() => void) | null = null;
  onerror: (() => void) | null = null;
  onmessage: ((event: { data: unknown }) => void) | null = null;
  readonly sent: string[] = [];

  send(data: string): void {
    this.sent.push(data);
  }
  close(): void {}
  simulateMessage(data: unknown): void {
    this.onmessage?.({ data });
  }
  lastSentFrame(): unknown {
    return JSON.parse(this.sent[this.sent.length - 1] ?? "null");
  }
  framesNamed(key: string): unknown[] {
    return this.sent.map((frame) => JSON.parse(frame)).filter((frame) => key in (frame as object));
  }
}

class FakePeerConnection implements RTCPeerConnectionLike {
  readonly tracks: MediaStreamTrack[] = [];
  closed = false;
  localDescription: { type: string; sdp: string } | null = null;
  remoteDescription: { type: string; sdp: string } | null = null;
  iceGatheringState = "complete";
  ontrack: ((event: { track: MediaStreamTrack; streams: MediaStream[] }) => void) | null = null;
  onicegatheringstatechange: (() => void) | null = null;

  addTrack(track: MediaStreamTrack): unknown {
    this.tracks.push(track);
    return {};
  }
  async createOffer(): Promise<{ type: "offer"; sdp: string }> {
    return { type: "offer", sdp: "fake-offer-sdp" };
  }
  async createAnswer(): Promise<{ type: "answer"; sdp: string }> {
    return { type: "answer", sdp: "fake-answer-sdp" };
  }
  async setLocalDescription(description: { type: string; sdp: string }): Promise<void> {
    this.localDescription = description;
  }
  async setRemoteDescription(description: { type: string; sdp: string }): Promise<void> {
    this.remoteDescription = description;
  }
  close(): void {
    this.closed = true;
  }
}

function fakeTrack(kind: "audio" | "video"): MediaStreamTrack {
  return { kind, enabled: true, stop: vi.fn(), onended: null } as unknown as MediaStreamTrack;
}

function fakeStream(...tracks: MediaStreamTrack[]): MediaStream {
  return {
    getAudioTracks: () => tracks.filter((t) => t.kind === "audio"),
    getVideoTracks: () => tracks.filter((t) => t.kind === "video"),
  } as unknown as MediaStream;
}

function setup() {
  const socket = new FakeWebSocket();
  const wsFactory: WebSocketFactory = vi.fn().mockReturnValue(socket);
  const chatClient = new ChatClient("wss://chat.example.test", wsFactory);
  chatClient.connectToChannel("t1", 1);

  const connections: FakePeerConnection[] = [];
  const pcFactory = vi.fn(() => {
    const pc = new FakePeerConnection();
    connections.push(pc);
    return pc;
  });
  const audioTrack = fakeTrack("audio");
  const getUserMedia = vi
    .fn()
    .mockImplementation((constraints: { audio?: boolean; video?: boolean }) =>
      Promise.resolve(constraints.video === true ? fakeStream(fakeTrack("video")) : fakeStream(audioTrack)),
    );
  const getDisplayMedia = vi.fn().mockResolvedValue(fakeStream(fakeTrack("video")));

  const manager = new CallManager(chatClient, "alice", pcFactory, getUserMedia, getDisplayMedia);
  return { socket, chatClient, manager, connections, getUserMedia, getDisplayMedia };
}

describe("CallManager", () => {
  it("joinCall() requests the microphone and sends call_join", async () => {
    const { socket, manager, getUserMedia } = setup();

    await manager.joinCall();

    expect(getUserMedia).toHaveBeenCalledWith({ audio: true });
    expect(socket.lastSentFrame()).toEqual({ call_join: true });
    expect(manager.inCall()).toBe(true);
  });

  it("emits 'error' but still joins if the microphone is unavailable", async () => {
    const { manager, getUserMedia } = setup();
    getUserMedia.mockRejectedValueOnce(new Error("denied"));
    const onError = vi.fn();
    manager.on("error", onError);

    await manager.joinCall();

    expect(onError).toHaveBeenCalledWith("Couldn't access the microphone.");
    expect(manager.inCall()).toBe(true);
  });

  // Issue #362 — without this, a participant joining the call second (or
  // later) never saw anyone already in it, only those who joined after
  // them: call_roster (this client's own call_join response) lists
  // pre-existing participants, separately from call_peer_joined (which
  // only fires for joins that happen after this client's own).
  it("emits 'participantJoined' for each name in the initial call_roster", async () => {
    const { socket, manager } = setup();
    const onParticipantJoined = vi.fn();
    manager.on("participantJoined", onParticipantJoined);

    await manager.joinCall();
    socket.simulateMessage(JSON.stringify({ call_roster: ["bob", "carol"], sfu_room: "channel-1" }));

    expect(onParticipantJoined).toHaveBeenCalledWith("bob");
    expect(onParticipantJoined).toHaveBeenCalledWith("carol");
    expect(onParticipantJoined).toHaveBeenCalledTimes(2);
  });

  it("emits 'participantJoined' when the server broadcasts call_peer_joined for someone who joins after", async () => {
    const { socket, manager } = setup();
    const onParticipantJoined = vi.fn();
    manager.on("participantJoined", onParticipantJoined);

    await manager.joinCall();
    socket.simulateMessage(JSON.stringify({ call_peer_joined: "dave" }));

    expect(onParticipantJoined).toHaveBeenCalledWith("dave");
  });

  it("creates a publish connection and attaches once sfuRoomAssigned fires", async () => {
    const { socket, manager, connections } = setup();
    await manager.joinCall();

    socket.simulateMessage(JSON.stringify({ call_roster: [], sfu_room: "room-1" }));

    expect(connections).toHaveLength(1);
    expect(socket.framesNamed("janus_attach")).toHaveLength(1);
  });

  it("joins the SFU room as publisher once attached, and negotiates once joined", async () => {
    const { socket, manager, connections } = setup();
    await manager.joinCall();
    socket.simulateMessage(JSON.stringify({ call_roster: [], sfu_room: "room-1" }));

    socket.simulateMessage(JSON.stringify({ janus_attached: { handle: 10 } }));

    const joinFrame = socket.framesNamed("janus_message")[0] as {
      janus_message: { handle: number; body: unknown };
    };
    expect(joinFrame.janus_message).toEqual({
      handle: 10,
      body: { request: "join", room: "room-1", ptype: "publisher", display: "alice" },
    });

    socket.simulateMessage(
      JSON.stringify({
        janus_event: {
          sender: 10,
          plugindata: { data: { videoroom: "joined", id: "feed-alice", publishers: [] } },
        },
      }),
    );
    await flushAsync();

    const configureFrame = socket
      .framesNamed("janus_message")
      .map((f) => (f as { janus_message: { body: { request: string } } }).janus_message)
      .find((m) => m.body.request === "configure");
    expect(configureFrame).toBeDefined();
    expect(connections[0]?.localDescription?.type).toBe("offer");
  });

  // Issue #364 — some real networks never fire iceGatheringState
  // "complete" at all (observed: a host with several virtual network
  // adapters alongside the real one, where at least one interface's own
  // STUN query never resolves). Waiting unconditionally for "complete"
  // before sending the offer meant the call never published anything on
  // such a network — negotiatePublish() just hung forever, silently,
  // before ever reaching the "configure" send. This fake connection
  // never reports "complete" on its own (no onicegatheringstatechange
  // call), mirroring that hang; without the bounded timeout in
  // waitForIceGatheringComplete(), advancing past kIceGatheringTimeoutMs
  // would never produce a "configure" frame.
  it("still sends 'configure' after a timeout if iceGatheringState never reaches 'complete'", async () => {
    vi.useFakeTimers();
    try {
      const { socket, manager, connections } = setup();
      await manager.joinCall();
      socket.simulateMessage(JSON.stringify({ call_roster: [], sfu_room: "room-1" }));

      const publishConnection = connections[0];
      expect(publishConnection).toBeDefined();
      if (publishConnection) {
        publishConnection.iceGatheringState = "checking";
      }

      socket.simulateMessage(JSON.stringify({ janus_attached: { handle: 10 } }));
      socket.simulateMessage(
        JSON.stringify({
          janus_event: {
            sender: 10,
            plugindata: { data: { videoroom: "joined", id: "feed-alice", publishers: [] } },
          },
        }),
      );
      await flushAsync();

      const hasConfigureBeforeTimeout = socket
        .framesNamed("janus_message")
        .map((f) => (f as { janus_message: { body: { request: string } } }).janus_message)
        .some((m) => m.body.request === "configure");
      expect(hasConfigureBeforeTimeout).toBe(false);

      await vi.advanceTimersByTimeAsync(kIceGatheringTimeoutMs);
      await flushAsync();

      const configureFrame = socket
        .framesNamed("janus_message")
        .map((f) => (f as { janus_message: { body: { request: string } } }).janus_message)
        .find((m) => m.body.request === "configure");
      expect(configureFrame).toBeDefined();
    } finally {
      vi.useRealTimers();
    }
  });

  it("subscribes to an existing publisher listed in the 'joined' event", async () => {
    const { socket, manager, connections } = setup();
    await manager.joinCall();
    socket.simulateMessage(JSON.stringify({ call_roster: [], sfu_room: "room-1" }));
    socket.simulateMessage(JSON.stringify({ janus_attached: { handle: 10 } })); // publish attach
    socket.simulateMessage(
      JSON.stringify({
        janus_event: {
          sender: 10,
          plugindata: {
            data: { videoroom: "joined", id: "feed-alice", publishers: [{ id: "feed-bob", display: "bob" }] },
          },
        },
      }),
    );

    // Second attach is for the subscribe connection to bob's feed.
    socket.simulateMessage(JSON.stringify({ janus_attached: { handle: 11 } }));

    expect(connections).toHaveLength(2);
    const subscribeFrame = socket
      .framesNamed("janus_message")
      .map((f) => (f as { janus_message: { handle: number; body: unknown } }).janus_message)
      .find((m) => m.handle === 11);
    expect(subscribeFrame?.body).toEqual({
      request: "join",
      room: "room-1",
      ptype: "subscriber",
      feed: "feed-bob",
    });
  });

  // Issue #373 — Janus's feed-based subscribe (`ptype: "subscriber", feed:
  // ...`) only hands a subscriber whatever the feed was publishing at
  // subscribe time; it doesn't push a stream the feed adds later (e.g.
  // the peer turns their camera on after this side already subscribed
  // to their audio-only feed) to an already-subscribed handle. Confirmed
  // by raw signaling log: Janus's own "publishers updated" notification
  // reaches every other participant's publish handle fine, but nothing
  // arrives on the existing subscribe handle unless this side explicitly
  // asks for the new stream. Without this, a peer who joined the call
  // before the other side turned their camera on would never see their
  // video at all — the tile exists (remoteStream already fired for
  // audio), but the video track never arrives.
  it("asks Janus to add a stream to an existing subscription when the feed's publisher list grows", async () => {
    const { socket, manager, connections } = setup();
    await manager.joinCall();
    socket.simulateMessage(JSON.stringify({ call_roster: [], sfu_room: "room-1" }));
    socket.simulateMessage(JSON.stringify({ janus_attached: { handle: 10 } }));
    socket.simulateMessage(
      JSON.stringify({
        janus_event: {
          sender: 10,
          plugindata: {
            data: {
              videoroom: "joined",
              id: "feed-alice",
              publishers: [{ id: "feed-bob", display: "bob", streams: [{ mid: "0", type: "audio" }] }],
            },
          },
        },
      }),
    );
    socket.simulateMessage(JSON.stringify({ janus_attached: { handle: 11 } })); // subscribe to bob

    // Bob turns his camera on — Janus broadcasts the updated publisher
    // list (now including a video stream) to every other participant's
    // *publish* handle, same as it did when bob first joined.
    socket.simulateMessage(
      JSON.stringify({
        janus_event: {
          sender: 10,
          plugindata: {
            data: {
              videoroom: "event",
              publishers: [
                {
                  id: "feed-bob",
                  display: "bob",
                  streams: [
                    { mid: "0", type: "audio" },
                    { mid: "1", type: "video" },
                  ],
                },
              ],
            },
          },
        },
      }),
    );

    // No new attach/connection — this reuses the existing subscribe
    // handle (11) rather than creating a second one for the same peer.
    expect(connections).toHaveLength(2);
    const subscribeUpdateFrame = socket
      .framesNamed("janus_message")
      .map((f) => (f as { janus_message: { handle: number; body: unknown } }).janus_message)
      .find((m) => m.handle === 11 && (m.body as { request?: string }).request === "subscribe");
    expect(subscribeUpdateFrame?.body).toEqual({
      request: "subscribe",
      streams: [{ feed: "feed-bob", mid: "1" }],
    });
  });

  it("emits 'remoteStream' when a subscribed peer's track arrives, and answers with an SDP", async () => {
    const { socket, manager, connections } = setup();
    const onRemoteStream = vi.fn();
    manager.on("remoteStream", onRemoteStream);
    await manager.joinCall();
    socket.simulateMessage(JSON.stringify({ call_roster: [], sfu_room: "room-1" }));
    socket.simulateMessage(JSON.stringify({ janus_attached: { handle: 10 } }));
    socket.simulateMessage(
      JSON.stringify({
        janus_event: {
          sender: 10,
          plugindata: {
            data: { videoroom: "joined", id: "feed-alice", publishers: [{ id: "feed-bob", display: "bob" }] },
          },
        },
      }),
    );
    socket.simulateMessage(JSON.stringify({ janus_attached: { handle: 11 } }));

    const subscribeConnection = connections[1];
    const remoteStream = fakeStream(fakeTrack("video"));
    subscribeConnection?.ontrack?.({ track: fakeTrack("video"), streams: [remoteStream] });

    expect(onRemoteStream).toHaveBeenCalledWith("bob", remoteStream);

    // Janus's jsep-offer for the subscription — should trigger an answer + "start".
    socket.simulateMessage(
      JSON.stringify({ janus_event: { sender: 11, jsep: { type: "offer", sdp: "remote-offer-sdp" } } }),
    );
    await flushAsync();

    expect(subscribeConnection?.remoteDescription).toEqual({ type: "offer", sdp: "remote-offer-sdp" });
    const startFrame = socket
      .framesNamed("janus_message")
      .map((f) => (f as { janus_message: { handle: number; body: { request: string } } }).janus_message)
      .find((m) => m.handle === 11 && m.body.request === "start");
    expect(startFrame).toBeDefined();
  });

  it("closes the peer's subscribe connection when it leaves the call", async () => {
    const { socket, manager, connections } = setup();
    const onLeft = vi.fn();
    const onRemoved = vi.fn();
    manager.on("participantLeft", onLeft);
    manager.on("remoteStreamRemoved", onRemoved);
    await manager.joinCall();
    socket.simulateMessage(JSON.stringify({ call_roster: [], sfu_room: "room-1" }));
    socket.simulateMessage(JSON.stringify({ janus_attached: { handle: 10 } }));
    socket.simulateMessage(
      JSON.stringify({
        janus_event: {
          sender: 10,
          plugindata: {
            data: { videoroom: "joined", id: "feed-alice", publishers: [{ id: "feed-bob", display: "bob" }] },
          },
        },
      }),
    );
    socket.simulateMessage(JSON.stringify({ janus_attached: { handle: 11 } }));

    socket.simulateMessage(JSON.stringify({ call_peer_left: "bob" }));

    expect(onLeft).toHaveBeenCalledWith("bob");
    expect(onRemoved).toHaveBeenCalledWith("bob");
    expect(connections[1]?.closed).toBe(true);
  });

  it("leaveCall() closes every connection and stops local tracks", async () => {
    const { socket, manager, connections } = setup();
    await manager.joinCall();
    socket.simulateMessage(JSON.stringify({ call_roster: [], sfu_room: "room-1" }));
    socket.simulateMessage(JSON.stringify({ janus_attached: { handle: 10 } }));

    manager.leaveCall();

    expect(socket.lastSentFrame()).toEqual({ call_leave: true });
    expect(connections[0]?.closed).toBe(true);
    expect(manager.inCall()).toBe(false);
  });

  it("setMuted()/isMuted() toggle the local audio track's enabled flag", async () => {
    const { manager } = setup();
    await manager.joinCall();

    expect(manager.isMuted()).toBe(false);
    manager.setMuted(true);
    expect(manager.isMuted()).toBe(true);
  });

  it("enableVideo() and enableScreenShare() are mutually exclusive", async () => {
    const { manager } = setup();
    const onCamera = vi.fn();
    const onScreen = vi.fn();
    manager.on("localCameraStream", onCamera);
    manager.on("localScreenShareStream", onScreen);
    await manager.joinCall();

    await manager.enableVideo();
    expect(manager.videoEnabled()).toBe(true);
    expect(onCamera).toHaveBeenLastCalledWith(expect.anything());

    await manager.enableScreenShare();
    expect(manager.videoEnabled()).toBe(false);
    expect(manager.screenShareEnabled()).toBe(true);
    expect(onCamera).toHaveBeenLastCalledWith(null);
    expect(onScreen).toHaveBeenLastCalledWith(expect.anything());
  });

  it("emits 'reactionReceived' when the server broadcasts another participant's call_reaction", async () => {
    const { socket, manager } = setup();
    const onReaction = vi.fn();
    manager.on("reactionReceived", onReaction);
    await manager.joinCall();

    socket.simulateMessage(JSON.stringify({ call_reaction: { login: "bob", emoji: "👍" } }));

    expect(onReaction).toHaveBeenCalledWith("bob", "👍");
  });

  it("sendReaction() sends call_reaction while in a call", async () => {
    const { socket, manager } = setup();
    await manager.joinCall();

    manager.sendReaction("👍");

    expect(socket.lastSentFrame()).toEqual({ call_reaction: "👍" });
  });

  it("sendReaction() is a no-op before joining a call", () => {
    const { socket, manager } = setup();

    manager.sendReaction("👍");

    expect(socket.sent).toHaveLength(0);
  });

  it("disableVideo()/disableScreenShare() only stop the matching kind", async () => {
    const { manager } = setup();
    await manager.joinCall();
    await manager.enableVideo();

    manager.disableScreenShare(); // no-op, screen-share isn't the active kind
    expect(manager.videoEnabled()).toBe(true);

    manager.disableVideo();
    expect(manager.videoEnabled()).toBe(false);
  });
});
