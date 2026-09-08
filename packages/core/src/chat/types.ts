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

export interface ChatMessageInfo {
  id: number;
  author: string;
  body: string;
  sentAt: string;
  attachmentId?: number;
  attachmentFilename?: string;
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
