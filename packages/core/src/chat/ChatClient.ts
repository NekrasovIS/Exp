// TypeScript port of src/chat/ChatClient.h/.cpp (issue #250) — the
// WebSocket half of the chat-service protocol. Ported from Qt signals to
// a small typed event-listener API (`on(event, listener)`), since
// there's no single "success" to resolve a promise with for a
// long-lived connection.
//
// Auth is carried in the first frame's body ("token"), not an HTTP
// header — a meaningful difference from every REST client in this
// package, which all send `Authorization: Bearer`.
//
// One instance subscribes to exactly one thing (a channel OR a DM
// thread) at a time, same as the C++ class's own doc comment — use two
// instances for simultaneous channel + DM-thread subscriptions.

import type { IncomingChatMessage } from "./types.js";

/** Minimal WebSocket surface this client needs — satisfied by both the
 * browser/RN global `WebSocket` and the `ws` package's `WebSocket`
 * class. No default implementation is provided (unlike `fetch` in the
 * REST clients): this package makes no assumption about which of those
 * three a given app should use, so the caller always chooses. */
export interface WebSocketLike {
  send(data: string): void;
  close(): void;
  onopen: (() => void) | null;
  onclose: (() => void) | null;
  onerror: (() => void) | null;
  onmessage: ((event: { data: unknown }) => void) | null;
}

export type WebSocketFactory = (url: string) => WebSocketLike;

interface ChatClientEventMap {
  subscribed: [id: number];
  message: [message: IncomingChatMessage];
  messageEdited: [id: number, newBody: string, editedAt: string];
  messageDeleted: [id: number];
  // issue #308/#338/#340 — pinnedBy/pinnedAt belong to the ORIGINAL pin
  // even if this particular event was triggered by a second,
  // idempotent pin_message from someone else (see chat-service's own
  // PinMessageResult doc comment).
  messagePinned: [id: number, pinnedBy: string, pinnedAt: string];
  messageUnpinned: [id: number];
  error: [message: string];
  callRoster: [participants: string[]];
  // SFU room for the call (issue #221/#232) — same call_join response as
  // callRoster, just its own event so callRoster's own signature (kept
  // for wire-format parity with the C++ side) doesn't need to change.
  // Not emitted if chat-service couldn't provision a room (Janus
  // temporarily unavailable).
  sfuRoomAssigned: [room: string];
  callPeerJoined: [login: string];
  callPeerLeft: [login: string];
  callSignal: [from: string, payload: unknown];
  // SFU signaling proxy (issue #221/#232) — see sendJanusAttach()/
  // sendJanusMessage()'s own doc comments for what each answers.
  janusAttached: [handle: number];
  janusMessageAck: [response: unknown];
  janusEvent: [event: Record<string, unknown>];
  userTyping: [login: string];
  // Presence (issue #322 — chat-service's own #309): community-wide,
  // not per-channel. onlineMembers is a second event on the same
  // "subscribed" response as subscribed above (mirrors sfuRoomAssigned's
  // own "second event on one response" shape) — logins already
  // connected to any channel of this subscription's community, not
  // including self. presenceChanged fires for every later
  // connect/disconnect elsewhere in that community. Neither fires for
  // a DM-thread subscription (dialogs have no community).
  onlineMembers: [logins: string[]];
  presenceChanged: [login: string, online: boolean];
}

type EventListener<K extends keyof ChatClientEventMap> = (...args: ChatClientEventMap[K]) => void;

interface PendingHello {
  token: string;
  channelId?: number;
  dmThreadId?: number;
}

export class ChatClient {
  private socket: WebSocketLike | null = null;
  private pending: PendingHello | null = null;
  // A `Map` rather than `{ [K in keyof ChatClientEventMap]?: Set<...> }`
  // deliberately — TypeScript can't verify a generically-keyed read/write
  // into a mapped object type is sound (indexing it with a generic `K`
  // distributes across the whole union instead of treating it as one
  // concrete key, collapsing the value type to an unsatisfiable
  // intersection). Type safety is enforced at the public on()/emit()
  // boundary instead, the same trade-off most typed event-emitter
  // libraries make internally.
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  private readonly listeners = new Map<keyof ChatClientEventMap, Set<(...args: any[]) => void>>();

  constructor(
    private readonly url: string,
    private readonly wsFactory: WebSocketFactory,
  ) {}

  on<K extends keyof ChatClientEventMap>(event: K, listener: EventListener<K>): () => void {
    let set = this.listeners.get(event);
    if (set === undefined) {
      set = new Set();
      this.listeners.set(event, set);
    }
    set.add(listener);
    return () => set.delete(listener);
  }

  connectToChannel(token: string, channelId: number): void {
    this.pending = { token, channelId };
    this.open();
  }

  connectToDirectMessageThread(token: string, dmThreadId: number): void {
    this.pending = { token, dmThreadId };
    this.open();
  }

  /** Closes the connection — valid whether currently subscribed to a
   * channel or a DM thread. */
  disconnect(): void {
    this.socket?.close();
    this.socket = null;
  }

  sendMessage(body: string, attachmentId?: number): void {
    const frame: Record<string, unknown> = { body };
    if (attachmentId !== undefined) {
      frame.attachment_id = attachmentId;
    }
    this.sendFrame(frame);
  }

  /** Valid only in channel mode — chat-service doesn't process this for
   * a DM-thread subscription. */
  joinCall(): void {
    this.sendFrame({ call_join: true });
  }

  /** Valid only in channel mode. */
  leaveCall(): void {
    this.sendFrame({ call_leave: true });
  }

  /** Valid only in channel mode. @p payload is opaque (SDP/ICE data),
   * never inspected here — just relayed by chat-service. */
  sendCallSignal(to: string, payload: unknown): void {
    this.sendFrame({ call_signal: { to, payload } });
  }

  /** SFU signaling proxy (issue #221/#232): attaches a new videoroom
   * plugin handle on this connection's Janus session (chat-service
   * creates the session itself on the first call over this
   * connection's lifetime). Answered by a `janusAttached` event. */
  sendJanusAttach(): void {
    this.sendFrame({ janus_attach: true });
  }

  /** Relays @p body (plus @p jsep, if given) verbatim to the given
   * Janus @p handle through chat-service — this method never inspects
   * their contents ("join"/"configure"/"subscribe"/"start" etc., see
   * the Janus videoroom protocol). The direct reply (may be just an
   * ack — the real result arrives asynchronously) comes through
   * `janusMessageAck`; async events on the same session (including a
   * jsep answer from Janus) come through `janusEvent`. */
  sendJanusMessage(handle: number, body: unknown, jsep?: unknown): void {
    const inner: Record<string, unknown> = { handle, body };
    if (jsep !== undefined) {
      inner.jsep = jsep;
    }
    this.sendFrame({ janus_message: inner });
  }

  /** Valid only in channel mode. Ephemeral — the server never echoes
   * this back to the sender. */
  sendTyping(): void {
    this.sendFrame({ typing: true });
  }

  /** Valid only in channel mode. The server only allows editing your own message. */
  sendEditMessage(id: number, newBody: string): void {
    this.sendFrame({ edit_message: { id, body: newBody } });
  }

  /** Valid only in channel mode. The server only allows deleting your own message. */
  sendDeleteMessage(id: number): void {
    this.sendFrame({ delete_message: { id } });
  }

  /** Valid only in channel mode. Unlike sendEditMessage()/
   * sendDeleteMessage(), the server restricts this to the channel/
   * community owner or a moderator — never the message's own author
   * as such (pinning is a channel-management action, not message
   * moderation). Idempotent — pinning an already-pinned message is a
   * no-op that still answers with a `messagePinned` event. */
  sendPinMessage(id: number): void {
    this.sendFrame({ pin_message: { id } });
  }

  /** Same authorization rule as sendPinMessage(); idempotent. */
  sendUnpinMessage(id: number): void {
    this.sendFrame({ unpin_message: { id } });
  }

  private open(): void {
    const socket = this.wsFactory(this.url);
    this.socket = socket;
    socket.onopen = () => {
      if (this.pending === null) {
        return;
      }
      const hello =
        this.pending.dmThreadId !== undefined
          ? { token: this.pending.token, dm_thread_id: this.pending.dmThreadId }
          : { token: this.pending.token, channel_id: this.pending.channelId };
      socket.send(JSON.stringify(hello));
    };
    socket.onerror = () => this.emit("error", "WebSocket transport error");
    socket.onmessage = (event) => this.handleMessage(String(event.data));
  }

  private sendFrame(frame: unknown): void {
    if (this.socket === null) {
      throw new Error(
        "ChatClient is not connected — call connectToChannel()/connectToDirectMessageThread() first",
      );
    }
    this.socket.send(JSON.stringify(frame));
  }

  private emit<K extends keyof ChatClientEventMap>(event: K, ...args: ChatClientEventMap[K]): void {
    this.listeners.get(event)?.forEach((listener) => listener(...args));
  }

  // Incoming frames are mutually exclusive by construction (mirrors the
  // C++ class's if/else-if chain) — the first matching shape below wins,
  // and a frame matching none of them is silently dropped, same as the
  // C++ side (no "unknown message" fallback event).
  private handleMessage(raw: string): void {
    let parsed: unknown;
    try {
      parsed = JSON.parse(raw);
    } catch {
      parsed = undefined;
    }
    if (typeof parsed !== "object" || parsed === null) {
      this.emit("error", "Malformed message from chat-service");
      return;
    }
    const body = parsed as Record<string, unknown>;

    if (typeof body.error === "string") {
      this.emit("error", body.error);
      return;
    }
    if (body.subscribed === true) {
      const id = typeof body.dm_thread_id === "number" ? body.dm_thread_id : body.channel_id;
      if (typeof id === "number") {
        this.emit("subscribed", id);
      }
      if (Array.isArray(body.online_members)) {
        this.emit("onlineMembers", body.online_members as string[]);
      }
      return;
    }
    if (typeof body.presence_changed === "object" && body.presence_changed !== null) {
      const presence = body.presence_changed as { login?: unknown; online?: unknown };
      if (typeof presence.login === "string" && typeof presence.online === "boolean") {
        this.emit("presenceChanged", presence.login, presence.online);
        return;
      }
    }
    if (Array.isArray(body.call_roster)) {
      this.emit("callRoster", body.call_roster as string[]);
      // Same call_join response as callRoster above, not a separate
      // frame — see sfuRoomAssigned's own doc comment on the event map.
      if (typeof body.sfu_room === "string") {
        this.emit("sfuRoomAssigned", body.sfu_room);
      }
      return;
    }
    if (typeof body.call_peer_joined === "string") {
      this.emit("callPeerJoined", body.call_peer_joined);
      return;
    }
    if (typeof body.call_peer_left === "string") {
      this.emit("callPeerLeft", body.call_peer_left);
      return;
    }
    if (typeof body.call_signal === "object" && body.call_signal !== null) {
      const signal = body.call_signal as { from?: unknown; payload?: unknown };
      if (typeof signal.from === "string") {
        this.emit("callSignal", signal.from, signal.payload);
        return;
      }
    }
    if (typeof body.janus_attached === "object" && body.janus_attached !== null) {
      const attached = body.janus_attached as { handle?: unknown };
      if (typeof attached.handle === "number") {
        this.emit("janusAttached", attached.handle);
        return;
      }
    }
    if (typeof body.janus_message_ack === "object" && body.janus_message_ack !== null) {
      this.emit("janusMessageAck", body.janus_message_ack);
      return;
    }
    if (typeof body.janus_event === "object" && body.janus_event !== null) {
      this.emit("janusEvent", body.janus_event as Record<string, unknown>);
      return;
    }
    if (typeof body.user_typing === "string") {
      this.emit("userTyping", body.user_typing);
      return;
    }
    if (typeof body.message_edited === "object" && body.message_edited !== null) {
      const edited = body.message_edited as { id?: unknown; body?: unknown; edited_at?: unknown };
      if (
        typeof edited.id === "number" &&
        typeof edited.body === "string" &&
        typeof edited.edited_at === "string"
      ) {
        this.emit("messageEdited", edited.id, edited.body, edited.edited_at);
        return;
      }
    }
    if (typeof body.message_deleted === "object" && body.message_deleted !== null) {
      const deleted = body.message_deleted as { id?: unknown };
      if (typeof deleted.id === "number") {
        this.emit("messageDeleted", deleted.id);
        return;
      }
    }
    if (typeof body.message_pinned === "object" && body.message_pinned !== null) {
      const pinned = body.message_pinned as { id?: unknown; pinned_by?: unknown; pinned_at?: unknown };
      if (
        typeof pinned.id === "number" &&
        typeof pinned.pinned_by === "string" &&
        typeof pinned.pinned_at === "string"
      ) {
        this.emit("messagePinned", pinned.id, pinned.pinned_by, pinned.pinned_at);
        return;
      }
    }
    if (typeof body.message_unpinned === "object" && body.message_unpinned !== null) {
      const unpinned = body.message_unpinned as { id?: unknown };
      if (typeof unpinned.id === "number") {
        this.emit("messageUnpinned", unpinned.id);
        return;
      }
    }
    if (typeof body.author === "string" && typeof body.body === "string") {
      const message: IncomingChatMessage = {
        id: typeof body.id === "number" ? body.id : 0,
        author: body.author,
        body: body.body,
        sentAt: typeof body.sent_at === "string" ? body.sent_at : "",
      };
      if (typeof body.attachment_id === "number") {
        message.attachmentId = body.attachment_id;
      }
      if (typeof body.attachment_filename === "string") {
        message.attachmentFilename = body.attachment_filename;
      }
      this.emit("message", message);
    }
    // Anything else is silently dropped — no "unknown message" fallback.
  }
}
