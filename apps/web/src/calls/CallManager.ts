// Browser-side group calls (issue #221) — SFU via the same Janus
// videoroom proxy protocol chat-service already speaks to DeviceHub's
// desktop CallManager (src/chat/CallManager.h/.cpp, issue #232), not a
// separate mesh implementation: a web participant and a desktop
// participant in the same call publish/subscribe through the same
// Janus room, so they can actually see/hear each other. Mirrors that
// class's publish/subscribe state machine closely enough to cross-
// reference its doc comments for the *why* behind each step; this file
// only restates the *what* where it genuinely differs.
//
// Two deliberate simplifications versus the desktop client, both
// forced by real browser API limits, not by choice:
//
//  1. Camera and screen-share are mutually exclusive here (enabling one
//     disables the other), unlike desktop's independent toggles (issue
//     #185). Desktop tells its own camera/screen tracks apart on the
//     *receiving* end by a fixed track id it assigns at creation
//     (`kScreenShareTrackId` in CallManager.cpp) — libwebrtc's
//     `CreateVideoTrack(source, id)` takes an explicit id and Janus's
//     relay preserves it end to end. The browser's `MediaStreamTrack`
//     has no such constructor — its `id` is always a UA-generated
//     random UUID, so there is no reliable wire-level signal a browser
//     subscriber (or a desktop one looking at *this* browser's feed)
//     could use to tell two simultaneous video tracks in one feed
//     apart. Publishing at most one keeps this feed unambiguous for
//     every kind of viewer.
//  2. A side effect of (1): if a *desktop* peer has both camera and
//     screen-share on at once (it can, per #185), a browser subscriber
//     here only renders whichever video track's `ontrack` fires first
//     on that peer's subscribe connection — the other stays invisible
//     until the first one ends. Rare in practice (most calls don't mix
//     both), and no worse than not supporting the peer at all; documented
//     here rather than solved, since solving it needs Janus's
//     multistream `descriptions` metadata, which desktop doesn't send
//     either (changing that is its own cross-client issue, not this one's).
//
// ICE is non-trickle here too, matching chat-service/Janus's existing
// contract with the desktop client (see PeerObserver::OnIceGatheringChange()'s
// doc comment on the C++ side): candidates are never sent one at a time,
// the caller instead waits for `RTCPeerConnection.iceGatheringState` to
// reach "complete" and only then reads back `localDescription.sdp`,
// which by then has every candidate already folded in.

import { ChatClient } from "@devicehub/core";

/** The minimal `RTCPeerConnection` surface this class needs — satisfied
 * by the real browser global as-is. Narrowed for the same reason
 * ChatClient narrows WebSocket to `WebSocketLike`: so tests can supply a
 * fake without stubbing the entire (huge) real interface. */
export interface SessionDescriptionLike {
  type: RTCSdpType;
  sdp?: string;
}

export interface RTCPeerConnectionLike {
  addTrack(track: MediaStreamTrack): unknown;
  createOffer(): Promise<SessionDescriptionLike>;
  createAnswer(): Promise<SessionDescriptionLike>;
  setLocalDescription(description: SessionDescriptionLike): Promise<void>;
  setRemoteDescription(description: SessionDescriptionLike): Promise<void>;
  close(): void;
  readonly iceGatheringState: string;
  readonly localDescription: SessionDescriptionLike | null;
  ontrack: ((event: { track: MediaStreamTrack; streams: MediaStream[] }) => void) | null;
  onicegatheringstatechange: (() => void) | null;
}

export type RTCPeerConnectionFactory = () => RTCPeerConnectionLike;
export type GetUserMedia = (constraints: { audio?: boolean; video?: boolean }) => Promise<MediaStream>;
export type GetDisplayMedia = (constraints: { video?: boolean }) => Promise<MediaStream>;

interface CallManagerEventMap {
  participantJoined: [login: string];
  participantLeft: [login: string];
  /** A remote peer's media arrived (or was replaced) — @p stream carries
   * whatever audio/video tracks are currently on that peer's subscribe
   * connection (see the class doc comment's simplification #2 for the
   * video-track-count caveat). */
  remoteStream: [peerLogin: string, stream: MediaStream];
  remoteStreamRemoved: [peerLogin: string];
  localCameraStream: [stream: MediaStream | null];
  localScreenShareStream: [stream: MediaStream | null];
  /** Another participant reacted (issue #312/#328) — never fires for
   * this client's own sendReaction(), same as chat-service never
   * echoes it back to the sender. */
  reactionReceived: [login: string, emoji: string];
  error: [message: string];
}

type EventListener<K extends keyof CallManagerEventMap> = (...args: CallManagerEventMap[K]) => void;

type PendingAttach = "none" | "publish" | "subscribe";
type VideoKind = "camera" | "screen";

interface PeerEntry {
  connection: RTCPeerConnectionLike;
  janusHandle: number;
  feedId: string;
}

const kStunServers = [{ urls: "stun:stun.l.google.com:19302" }];

// Issue #364 — some real networks (observed: a host with several virtual
// adapters — Docker/VPN/WSL-style — alongside the real one) never fire
// iceGatheringState "complete" at all: Chromium appears to wait on every
// interface's own STUN query settling, and if even one of them (an
// unreachable IPv6 route, an interface with no real connectivity, etc.)
// never resolves, gathering hangs indefinitely — confirmed waiting 60s+
// with zero effect. Waiting forever for "complete" before ever sending
// this side's offer to Janus (this protocol's own non-trickle contract,
// see the class doc comment) meant the call never actually published
// anything on such a network — no error, no timeout, just permanently
// stuck before the first "configure" message. A bounded wait lets the
// offer go out with whatever candidates arrived in time — in every
// normal case that's still the complete set (a working STUN round trip
// typically finishes in well under a second), and on a broken network
// it trades "some candidates missing" for "actually attempts the call"
// — the same tradeoff a several-second non-trickle wait always makes
// relative to true trickle ICE, just bounded instead of unbounded.
export const kIceGatheringTimeoutMs = 2000;

function waitForIceGatheringComplete(connection: RTCPeerConnectionLike): Promise<void> {
  return new Promise((resolve) => {
    if (connection.iceGatheringState === "complete") {
      resolve();
      return;
    }
    const finish = (): void => {
      connection.onicegatheringstatechange = null;
      clearTimeout(timer);
      resolve();
    };
    const timer = setTimeout(finish, kIceGatheringTimeoutMs);
    connection.onicegatheringstatechange = () => {
      if (connection.iceGatheringState === "complete") {
        finish();
      }
    };
  });
}

export class CallManager {
  private sfuRoom: string | null = null;
  private ownFeedId: string | null = null;
  private publishConnection: RTCPeerConnectionLike | null = null;
  private publishHandle = -1;
  private localAudioTrack: MediaStreamTrack | null = null;
  private localVideoTrack: MediaStreamTrack | null = null;
  private videoKind: VideoKind | null = null;
  private readonly peers = new Map<string, PeerEntry>();
  private pendingAttach: PendingAttach = "none";
  private pendingSubscribe: { feedId: string; peerLogin: string } | null = null;
  private subscribeQueue: { feedId: string; peerLogin: string }[] = [];
  private inCallState = false;
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  private readonly listeners = new Map<keyof CallManagerEventMap, Set<(...args: any[]) => void>>();
  private readonly unsubscribes: (() => void)[] = [];

  constructor(
    private readonly chatClient: ChatClient,
    private readonly localLogin: string,
    // The real RTCPeerConnection satisfies RTCPeerConnectionLike's actual
    // runtime behavior but not its exact structural type (its ontrack
    // event carries many more fields than the narrow shape this class
    // reads, and its SDP types are nominal DOM types) — cast at this
    // one production boundary rather than widen the interface (and lose
    // the point of narrowing it for testability in the first place).
    private readonly pcFactory: RTCPeerConnectionFactory = () =>
      new RTCPeerConnection({ iceServers: kStunServers }) as unknown as RTCPeerConnectionLike,
    private readonly getUserMedia: GetUserMedia = (constraints) =>
      navigator.mediaDevices.getUserMedia(constraints),
    private readonly getDisplayMedia: GetDisplayMedia = (constraints) =>
      navigator.mediaDevices.getDisplayMedia(constraints),
  ) {
    this.unsubscribes.push(
      chatClient.on("sfuRoomAssigned", (room) => this.onSfuRoomAssigned(room)),
      // Issue #362 — the initial roster of who's already in the call
      // (this client's own join_call response, arriving before any
      // callPeerJoined for participants who join after). Without this,
      // a participant joining second or later never saw anyone who was
      // already in the call — only those who joined after them. Purely
      // informational, same as callPeerJoined below: the real
      // connection to each of them is entirely driven by Janus's own
      // publishers list (see onJanusEvent()), not this roster.
      chatClient.on("callRoster", (participants) => {
        for (const login of participants) {
          this.emit("participantJoined", login);
        }
      }),
      chatClient.on("callPeerJoined", (login) => this.emit("participantJoined", login)),
      chatClient.on("callPeerLeft", (login) => this.onCallPeerLeft(login)),
      chatClient.on("janusAttached", (handle) => this.onJanusAttached(handle)),
      chatClient.on("janusEvent", (event) => this.onJanusEvent(event)),
      chatClient.on("callReaction", (login, emoji) => this.emit("reactionReceived", login, emoji)),
    );
  }

  on<K extends keyof CallManagerEventMap>(event: K, listener: EventListener<K>): () => void {
    let set = this.listeners.get(event);
    if (set === undefined) {
      set = new Set();
      this.listeners.set(event, set);
    }
    set.add(listener);
    return () => set.delete(listener);
  }

  inCall(): boolean {
    return this.inCallState;
  }

  isMuted(): boolean {
    return this.localAudioTrack !== null && !this.localAudioTrack.enabled;
  }

  videoEnabled(): boolean {
    return this.videoKind === "camera";
  }

  screenShareEnabled(): boolean {
    return this.videoKind === "screen";
  }

  /** Joins the call in whichever channel `chatClient` is currently
   * subscribed to. Requests the microphone immediately — a call with no
   * audio at all isn't a real fallback here, so a denied/unavailable mic
   * surfaces as an `error` event but does not stop the join (matches
   * desktop's own callError()-not-a-hard-failure precedent for a null
   * audio device). */
  async joinCall(): Promise<void> {
    if (this.inCallState) {
      return;
    }
    this.inCallState = true;
    try {
      const stream = await this.getUserMedia({ audio: true });
      this.localAudioTrack = stream.getAudioTracks()[0] ?? null;
    } catch {
      this.emit("error", "Couldn't access the microphone.");
    }
    this.chatClient.joinCall();
  }

  leaveCall(): void {
    if (!this.inCallState) {
      return;
    }
    this.inCallState = false;
    this.chatClient.leaveCall();
    if (this.publishConnection) {
      this.publishConnection.close();
      this.publishConnection = null;
    }
    for (const peerLogin of [...this.peers.keys()]) {
      this.closeSubscribeConnection(peerLogin);
    }
    this.localAudioTrack?.stop();
    this.localAudioTrack = null;
    this.stopLocalVideo();
    this.sfuRoom = null;
    this.ownFeedId = null;
    this.publishHandle = -1;
    this.pendingAttach = "none";
    this.pendingSubscribe = null;
    this.subscribeQueue = [];
  }

  setMuted(muted: boolean): void {
    if (this.localAudioTrack) {
      this.localAudioTrack.enabled = !muted;
    }
  }

  /** No-op while not in a call (issue #312/#328, mirrors DeviceHub's
   * own CallManager::sendReaction() guard) rather than letting
   * chat-service reject it with an error the caller has nowhere
   * meaningful to surface. */
  sendReaction(emoji: string): void {
    if (this.inCallState) {
      this.chatClient.sendCallReaction(emoji);
    }
  }

  async enableVideo(): Promise<void> {
    await this.setLocalVideo("camera", () => this.getUserMedia({ video: true }));
  }

  disableVideo(): void {
    if (this.videoKind === "camera") {
      this.stopLocalVideo();
    }
  }

  async enableScreenShare(): Promise<void> {
    await this.setLocalVideo("screen", () => this.getDisplayMedia({ video: true }));
  }

  disableScreenShare(): void {
    if (this.videoKind === "screen") {
      this.stopLocalVideo();
    }
  }

  /** Releases everything and detaches from `chatClient` — call when the
   * owning component unmounts (e.g. leaving the channel view). Does
   * *not* itself call leaveCall()/chatClient.leaveCall() — the caller
   * decides whether unmounting mid-call should also hang up, same
   * separation ChatView/useChatSocket already draws between "stop
   * listening" and "tell the server". */
  dispose(): void {
    for (const off of this.unsubscribes) {
      off();
    }
  }

  private async setLocalVideo(kind: VideoKind, capture: () => Promise<MediaStream>): Promise<void> {
    this.stopLocalVideo();
    let stream: MediaStream;
    try {
      stream = await capture();
    } catch {
      this.emit(
        "error",
        kind === "camera" ? "Couldn't access the camera." : "Couldn't start screen sharing.",
      );
      return;
    }
    const track = stream.getVideoTracks()[0];
    if (track === undefined) {
      return;
    }
    this.localVideoTrack = track;
    this.videoKind = kind;
    // getDisplayMedia's track ends itself if the user stops sharing from
    // the browser's own "Stop sharing" bar rather than this app's UI —
    // without this, videoKind/the publish connection would silently
    // disagree with reality until the next explicit toggle.
    track.onended = () => {
      if (this.videoKind === kind) {
        this.stopLocalVideo();
      }
    };
    this.emit(kind === "camera" ? "localCameraStream" : "localScreenShareStream", stream);
    if (this.publishConnection) {
      this.publishConnection.addTrack(track);
      await this.negotiatePublish();
    }
  }

  private stopLocalVideo(): void {
    if (!this.localVideoTrack) {
      return;
    }
    const kind = this.videoKind;
    this.localVideoTrack.stop();
    this.localVideoTrack = null;
    this.videoKind = null;
    if (kind === "camera") {
      this.emit("localCameraStream", null);
    } else if (kind === "screen") {
      this.emit("localScreenShareStream", null);
    }
    if (this.publishConnection) {
      void this.negotiatePublish();
    }
  }

  private onSfuRoomAssigned(room: string): void {
    this.sfuRoom = room;
    this.ensurePublishConnection();
  }

  private ensurePublishConnection(): void {
    if (this.publishConnection || this.sfuRoom === null) {
      return;
    }
    const connection = this.pcFactory();
    this.publishConnection = connection;
    if (this.localAudioTrack) {
      connection.addTrack(this.localAudioTrack);
    }
    if (this.localVideoTrack) {
      connection.addTrack(this.localVideoTrack);
    }
    this.pendingAttach = "publish";
    this.chatClient.sendJanusAttach();
  }

  private ensureSubscribeConnection(feedId: string, peerLogin: string): void {
    if (
      feedId === "" ||
      peerLogin === "" ||
      feedId === this.ownFeedId ||
      peerLogin === this.localLogin ||
      this.peers.has(peerLogin)
    ) {
      return;
    }
    if (this.pendingAttach !== "none") {
      this.subscribeQueue.push({ feedId, peerLogin });
      return;
    }
    this.pendingAttach = "subscribe";
    this.pendingSubscribe = { feedId, peerLogin };
    this.chatClient.sendJanusAttach();
  }

  private processNextQueuedSubscribe(): void {
    while (this.pendingAttach === "none" && this.subscribeQueue.length > 0) {
      const next = this.subscribeQueue.shift();
      if (next === undefined) {
        break;
      }
      if (this.peers.has(next.peerLogin)) {
        continue; // subscribed by some other path while queued — take the next one
      }
      this.pendingAttach = "subscribe";
      this.pendingSubscribe = next;
      this.chatClient.sendJanusAttach();
      return;
    }
  }

  private closeSubscribeConnection(peerLogin: string): void {
    const entry = this.peers.get(peerLogin);
    if (entry === undefined) {
      return;
    }
    entry.connection.close();
    this.peers.delete(peerLogin);
    this.emit("remoteStreamRemoved", peerLogin);
  }

  private closeSubscribeConnectionByFeed(feedId: string): void {
    for (const [peerLogin, entry] of this.peers) {
      if (entry.feedId === feedId) {
        this.closeSubscribeConnection(peerLogin);
        return;
      }
    }
  }

  private onCallPeerLeft(login: string): void {
    this.closeSubscribeConnection(login);
    this.emit("participantLeft", login);
  }

  private onJanusAttached(handle: number): void {
    if (this.pendingAttach === "publish") {
      this.publishHandle = handle;
      this.pendingAttach = "none";
      this.chatClient.sendJanusMessage(handle, {
        request: "join",
        room: this.sfuRoom,
        ptype: "publisher",
        display: this.localLogin,
      });
      this.processNextQueuedSubscribe();
      return;
    }
    if (this.pendingAttach === "subscribe" && this.pendingSubscribe !== null) {
      const { feedId, peerLogin } = this.pendingSubscribe;
      this.pendingAttach = "none";
      this.pendingSubscribe = null;
      const connection = this.pcFactory();
      connection.ontrack = (event) => {
        this.emit("remoteStream", peerLogin, event.streams[0] ?? new MediaStream([event.track]));
      };
      this.peers.set(peerLogin, { connection, janusHandle: handle, feedId });
      this.chatClient.sendJanusMessage(handle, {
        request: "join",
        room: this.sfuRoom,
        ptype: "subscriber",
        feed: feedId,
      });
      this.processNextQueuedSubscribe();
    }
  }

  private async negotiatePublish(): Promise<void> {
    const connection = this.publishConnection;
    if (connection === null || this.publishHandle < 0) {
      return;
    }
    const offer = await connection.createOffer();
    await connection.setLocalDescription(offer);
    await waitForIceGatheringComplete(connection);
    const localDescription = connection.localDescription;
    if (localDescription === null) {
      return;
    }
    this.chatClient.sendJanusMessage(
      this.publishHandle,
      { request: "configure" },
      { type: localDescription.type, sdp: localDescription.sdp },
    );
  }

  private async answerSubscribe(peerLogin: string, offerSdp: string | undefined): Promise<void> {
    const entry = this.peers.get(peerLogin);
    if (entry === undefined) {
      return;
    }
    await entry.connection.setRemoteDescription({ type: "offer", sdp: offerSdp ?? "" });
    const answer = await entry.connection.createAnswer();
    await entry.connection.setLocalDescription(answer);
    await waitForIceGatheringComplete(entry.connection);
    const localDescription = entry.connection.localDescription;
    if (localDescription === null) {
      return;
    }
    this.chatClient.sendJanusMessage(
      entry.janusHandle,
      { request: "start", room: this.sfuRoom },
      { type: localDescription.type, sdp: localDescription.sdp },
    );
  }

  private onJanusEvent(event: Record<string, unknown>): void {
    const sender = typeof event.sender === "number" ? event.sender : -1;
    const plugindata = event.plugindata as { data?: Record<string, unknown> } | undefined;
    const data = plugindata?.data ?? {};
    const videoroom = typeof data.videoroom === "string" ? data.videoroom : "";
    const jsep = event.jsep as SessionDescriptionLike | undefined;

    if (this.publishHandle >= 0 && sender === this.publishHandle) {
      if (videoroom === "joined") {
        this.ownFeedId = typeof data.id === "string" ? data.id : String(data.id ?? "");
        this.subscribeToPublishers(data.publishers);
        void this.negotiatePublish();
      } else if (videoroom === "event") {
        this.subscribeToPublishers(data.publishers);
        const leavingFeed =
          typeof data.leaving === "string"
            ? data.leaving
            : typeof data.unpublished === "string"
              ? data.unpublished
              : "";
        if (leavingFeed !== "") {
          this.closeSubscribeConnectionByFeed(leavingFeed);
        }
      }
      if (jsep !== undefined && this.publishConnection) {
        void this.publishConnection.setRemoteDescription(jsep);
      }
      return;
    }

    if (jsep === undefined) {
      return;
    }
    for (const [peerLogin, entry] of this.peers) {
      if (entry.janusHandle === sender) {
        void this.answerSubscribe(peerLogin, jsep.sdp);
        break;
      }
    }
  }

  private subscribeToPublishers(publishers: unknown): void {
    if (!Array.isArray(publishers)) {
      return;
    }
    for (const publisherValue of publishers) {
      const publisher = publisherValue as { id?: unknown; display?: unknown };
      const id = typeof publisher.id === "string" ? publisher.id : String(publisher.id ?? "");
      const display = typeof publisher.display === "string" ? publisher.display : "";
      this.ensureSubscribeConnection(id, display);
    }
  }

  private emit<K extends keyof CallManagerEventMap>(event: K, ...args: CallManagerEventMap[K]): void {
    this.listeners.get(event)?.forEach((listener) => listener(...args));
  }
}
