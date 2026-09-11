// Wire types for chat-service (issue #250) — mirror
// src/chat/ChatRestClient.h/.cpp and src/chat/ChatClient.h/.cpp.

export interface ChatItem {
  id: number;
  name: string;
  ownerLogin: string;
  isEncrypted: boolean;
  /** Only populated for communities the caller owns/is a member of, per
   * the C++ client's own doc comment. */
  inviteCode?: string;
}

/** One emoji's aggregated reactions on a message (issue #305/#333/#335)
 * — `logins` is the full list of who reacted with THIS emoji, exactly
 * as chat-service sends it (never a delta the caller has to merge). */
export interface MessageReactionInfo {
  emoji: string;
  logins: string[];
}

export interface ChatMessageInfo {
  id: number;
  author: string;
  body: string;
  sentAt: string;
  attachmentId?: number;
  attachmentFilename?: string;
  /** Always present (possibly empty), unlike attachmentId/attachmentFilename
   * — chat-service's own `toJson(Message)` always includes the
   * `reactions` field, even for a message with none. */
  reactions: MessageReactionInfo[];
}

/** A pinned channel message (issue #308/#338/#340) — full message
 * content plus who/when pinned it, as returned by
 * `GET /channels/{id}/pinned-messages`. Deliberately not an extension
 * of {@link ChatMessageInfo} with optional pin fields: pin metadata
 * only ever exists alongside a full message, never independently. */
export interface PinnedMessageInfo {
  id: number;
  author: string;
  body: string;
  sentAt: string;
  attachmentId?: number;
  attachmentFilename?: string;
  pinnedBy: string;
  pinnedAt: string;
}

export interface DirectMessageThreadInfo {
  id: number;
  otherLogin: string;
  createdAt: string;
}

export interface DirectMessageInfo {
  id: number;
  author: string;
  body: string;
  sentAt: string;
}

/** A message delivered over the WebSocket, as opposed to fetched via
 * REST — same shape as {@link ChatMessageInfo} minus nothing, kept as a
 * distinct alias so call sites reading ChatClient's `message` event
 * don't need to import ChatRestClient's type for what's conceptually a
 * different (live) source. */
export type IncomingChatMessage = ChatMessageInfo;
