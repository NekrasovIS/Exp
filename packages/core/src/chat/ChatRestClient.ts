// TypeScript port of src/chat/ChatRestClient.h/.cpp (issue #250) — same
// endpoints/fields; every method here requires a bearer token. The
// WebSocket side (ChatClient.h/.cpp) is ported separately in ChatClient.ts.

import {
  ApiError,
  extractErrorMessage,
  jsonRequestInit,
  readJsonBody,
  requestJson,
  resolveUrl,
} from "../http.js";
import type { FetchLike } from "../http.js";
import { toBase64 } from "./base64.js";
import type {
  ChatItem,
  ChatMessageInfo,
  DirectMessageInfo,
  DirectMessageThreadInfo,
  MessageReactionInfo,
  PinnedMessageInfo,
} from "./types.js";

interface ChatItemBody {
  id: number;
  name: string;
  owner: string;
  is_encrypted?: boolean;
  invite_code?: string;
}

interface MessageReactionBody {
  emoji: string;
  logins: string[];
}

interface MessageBody {
  id: number;
  author: string;
  body: string;
  sent_at: string;
  attachment_id?: number | null;
  attachment_filename?: string | null;
  reply_to_message_id?: number | null;
  reactions?: MessageReactionBody[];
}

interface PinnedMessageBody extends MessageBody {
  pinned_by: string;
  pinned_at: string;
}

interface DirectMessageThreadBody {
  id: number;
  other_login: string;
  created_at: string;
}

interface DirectMessageBody {
  id: number;
  author: string;
  body: string;
  sent_at: string;
}

const kGenericError = "Unexpected error from chat-service";

function toChatItem(body: ChatItemBody): ChatItem {
  const item: ChatItem = {
    id: body.id,
    name: body.name,
    ownerLogin: body.owner,
    isEncrypted: body.is_encrypted ?? false,
  };
  if (body.invite_code !== undefined) {
    item.inviteCode = body.invite_code;
  }
  return item;
}

function toMessageReactionInfo(body: MessageReactionBody): MessageReactionInfo {
  return { emoji: body.emoji, logins: body.logins };
}

function toChatMessageInfo(body: MessageBody): ChatMessageInfo {
  const message: ChatMessageInfo = {
    id: body.id,
    author: body.author,
    body: body.body,
    sentAt: body.sent_at,
    reactions: (body.reactions ?? []).map(toMessageReactionInfo),
  };
  if (body.attachment_id != null) message.attachmentId = body.attachment_id;
  if (body.attachment_filename != null) message.attachmentFilename = body.attachment_filename;
  if (body.reply_to_message_id != null) message.replyToMessageId = body.reply_to_message_id;
  return message;
}

function toPinnedMessageInfo(body: PinnedMessageBody): PinnedMessageInfo {
  const pinned: PinnedMessageInfo = {
    id: body.id,
    author: body.author,
    body: body.body,
    sentAt: body.sent_at,
    pinnedBy: body.pinned_by,
    pinnedAt: body.pinned_at,
  };
  if (body.attachment_id != null) pinned.attachmentId = body.attachment_id;
  if (body.attachment_filename != null) pinned.attachmentFilename = body.attachment_filename;
  return pinned;
}

function toDirectMessageThreadInfo(body: DirectMessageThreadBody): DirectMessageThreadInfo {
  return { id: body.id, otherLogin: body.other_login, createdAt: body.created_at };
}

function toDirectMessageInfo(body: DirectMessageBody): DirectMessageInfo {
  return { id: body.id, author: body.author, body: body.body, sentAt: body.sent_at };
}

function messagesQuery(limit: number, beforeId?: number): string {
  const params = new URLSearchParams({ limit: String(limit) });
  if (beforeId !== undefined && beforeId >= 0) {
    params.set("before_id", String(beforeId));
  }
  return params.toString();
}

export class ChatRestClient {
  constructor(
    private readonly baseUrl: string,
    private readonly fetchImpl: FetchLike = fetch,
  ) {}

  // ---- Communities ----

  async createCommunity(token: string, name: string): Promise<ChatItem> {
    return this.expectItem(
      requestJson<ChatItemBody>(
        this.fetchImpl,
        resolveUrl(this.baseUrl, "/communities"),
        jsonRequestInit("POST", token, { name }),
      ),
    );
  }

  async listCommunities(token: string): Promise<ChatItem[]> {
    return this.expectItemArray(
      requestJson<ChatItemBody[]>(
        this.fetchImpl,
        resolveUrl(this.baseUrl, "/communities/mine"),
        jsonRequestInit("GET", token),
      ),
    );
  }

  async renameCommunity(token: string, communityId: number, newName: string): Promise<void> {
    await this.expectOk(
      requestJson(
        this.fetchImpl,
        resolveUrl(this.baseUrl, `/communities/${communityId}`),
        jsonRequestInit("PATCH", token, { name: newName }),
      ),
    );
  }

  async deleteCommunity(token: string, communityId: number): Promise<void> {
    await this.expectOk(
      requestJson(
        this.fetchImpl,
        resolveUrl(this.baseUrl, `/communities/${communityId}`),
        jsonRequestInit("DELETE", token),
      ),
    );
  }

  async joinCommunity(token: string, communityId: number): Promise<void> {
    await this.expectOk(
      requestJson(
        this.fetchImpl,
        resolveUrl(this.baseUrl, `/communities/${communityId}/join`),
        jsonRequestInit("POST", token),
      ),
    );
  }

  async joinCommunityByCode(token: string, code: string): Promise<{ id: number; name: string }> {
    const res = await requestJson<{ id?: number; name?: string }>(
      this.fetchImpl,
      resolveUrl(this.baseUrl, "/communities/join-by-code"),
      jsonRequestInit("POST", token, { code }),
    );
    if (!res.ok || res.body?.id === undefined || res.body.name === undefined) {
      throw new ApiError(res.status, extractErrorMessage(res.body) ?? kGenericError);
    }
    return { id: res.body.id, name: res.body.name };
  }

  async regenerateInviteCode(token: string, communityId: number): Promise<string> {
    const res = await requestJson<{ invite_code?: string }>(
      this.fetchImpl,
      resolveUrl(this.baseUrl, `/communities/${communityId}/invite/regenerate`),
      jsonRequestInit("POST", token),
    );
    if (!res.ok || res.body?.invite_code === undefined) {
      throw new ApiError(res.status, extractErrorMessage(res.body) ?? kGenericError);
    }
    return res.body.invite_code;
  }

  // ---- Channels ----

  /** The response only carries `{id, is_encrypted}` — @p name is echoed
   * back from the argument, not re-parsed from the server (matching
   * ChatRestClient::createChannel()'s own `channelCreated(id, name,
   * isEncrypted)` signal, which likewise never carries an owner login). */
  async createChannel(
    token: string,
    communityId: number,
    name: string,
    isEncrypted = false,
  ): Promise<{ id: number; name: string; isEncrypted: boolean }> {
    const res = await requestJson<{ id?: number; is_encrypted?: boolean }>(
      this.fetchImpl,
      resolveUrl(this.baseUrl, `/communities/${communityId}/channels`),
      jsonRequestInit("POST", token, { name, is_encrypted: isEncrypted }),
    );
    if (!res.ok || res.body?.id === undefined) {
      throw new ApiError(res.status, extractErrorMessage(res.body) ?? kGenericError);
    }
    return { id: res.body.id, name, isEncrypted: res.body.is_encrypted ?? isEncrypted };
  }

  async listChannels(token: string, communityId: number): Promise<ChatItem[]> {
    return this.expectItemArray(
      requestJson<ChatItemBody[]>(
        this.fetchImpl,
        resolveUrl(this.baseUrl, `/communities/${communityId}/channels`),
        jsonRequestInit("GET", token),
      ),
    );
  }

  async listMembers(token: string, communityId: number): Promise<string[]> {
    return this.expectStringArray(
      requestJson<string[]>(
        this.fetchImpl,
        resolveUrl(this.baseUrl, `/communities/${communityId}/members`),
        jsonRequestInit("GET", token),
      ),
    );
  }

  async renameChannel(token: string, channelId: number, newName: string): Promise<void> {
    await this.expectOk(
      requestJson(
        this.fetchImpl,
        resolveUrl(this.baseUrl, `/channels/${channelId}`),
        jsonRequestInit("PATCH", token, { name: newName }),
      ),
    );
  }

  async deleteChannel(token: string, channelId: number): Promise<void> {
    await this.expectOk(
      requestJson(
        this.fetchImpl,
        resolveUrl(this.baseUrl, `/channels/${channelId}`),
        jsonRequestInit("DELETE", token),
      ),
    );
  }

  // ---- Channel encryption keys (issue #138/#217) ----

  async setChannelKey(
    token: string,
    channelId: number,
    memberLogin: string,
    wrappedKey: string,
  ): Promise<void> {
    await this.expectOk(
      requestJson(
        this.fetchImpl,
        resolveUrl(this.baseUrl, `/channels/${channelId}/keys/${encodeURIComponent(memberLogin)}`),
        jsonRequestInit("PUT", token, { wrapped_key: wrappedKey }),
      ),
    );
  }

  /** @returns the caller's own wrapped key, or `null` if none has been
   * set for them yet — a 404 here is an expected, common outcome (e.g.
   * a member who joined after the channel was created), not an error;
   * mirrors ChatRestClient::fetchMyChannelKey()'s dedicated
   * myChannelKeyNotFound() signal rather than routing it through
   * errorOccurred(). */
  async fetchMyChannelKey(token: string, channelId: number): Promise<string | null> {
    const res = await requestJson<{ wrapped_key?: string }>(
      this.fetchImpl,
      resolveUrl(this.baseUrl, `/channels/${channelId}/keys/me`),
      jsonRequestInit("GET", token),
    );
    if (res.status === 404) {
      return null;
    }
    if (!res.ok || res.body?.wrapped_key === undefined) {
      throw new ApiError(res.status, extractErrorMessage(res.body) ?? kGenericError);
    }
    return res.body.wrapped_key;
  }

  // ---- Moderation ----

  async promoteModerator(token: string, communityId: number, targetLogin: string): Promise<void> {
    await this.expectOk(
      requestJson(
        this.fetchImpl,
        resolveUrl(this.baseUrl, `/communities/${communityId}/moderators`),
        jsonRequestInit("POST", token, { login: targetLogin }),
      ),
    );
  }

  async demoteModerator(token: string, communityId: number, targetLogin: string): Promise<void> {
    await this.expectOk(
      requestJson(
        this.fetchImpl,
        resolveUrl(this.baseUrl, `/communities/${communityId}/moderators/${encodeURIComponent(targetLogin)}`),
        jsonRequestInit("DELETE", token),
      ),
    );
  }

  async listModerators(token: string, communityId: number): Promise<string[]> {
    return this.expectStringArray(
      requestJson<string[]>(
        this.fetchImpl,
        resolveUrl(this.baseUrl, `/communities/${communityId}/moderators`),
        jsonRequestInit("GET", token),
      ),
    );
  }

  // ---- Messages ----

  async listMessages(
    token: string,
    channelId: number,
    limit: number,
    beforeId?: number,
  ): Promise<ChatMessageInfo[]> {
    const res = await requestJson<MessageBody[]>(
      this.fetchImpl,
      resolveUrl(this.baseUrl, `/channels/${channelId}/messages?${messagesQuery(limit, beforeId)}`),
      jsonRequestInit("GET", token),
    );
    if (!res.ok || !Array.isArray(res.body)) {
      throw new ApiError(res.status, extractErrorMessage(res.body) ?? kGenericError);
    }
    return res.body.map(toChatMessageInfo);
  }

  /** Background sidebar-preview fetch — failures are silently swallowed
   * (resolving to an empty array) rather than thrown, matching
   * ChatRestClient::fetchLatestMessage()'s doc comment: this shouldn't
   * raise a toast/error per channel just because one preview failed. */
  async fetchLatestMessage(token: string, channelId: number): Promise<ChatMessageInfo[]> {
    try {
      const res = await requestJson<MessageBody[]>(
        this.fetchImpl,
        resolveUrl(this.baseUrl, `/channels/${channelId}/messages?limit=1`),
        jsonRequestInit("GET", token),
      );
      if (!res.ok || !Array.isArray(res.body)) {
        return [];
      }
      return res.body.map(toChatMessageInfo);
    } catch {
      return [];
    }
  }

  /** Any channel member may read the pinned list (issue #308/#338/#340)
   * — pinning/unpinning itself is a stricter owner-or-moderator-only
   * action, sent over the WebSocket (see {@link ChatClient.sendPinMessage}),
   * not this REST client. */
  async listPinnedMessages(token: string, channelId: number): Promise<PinnedMessageInfo[]> {
    const res = await requestJson<PinnedMessageBody[]>(
      this.fetchImpl,
      resolveUrl(this.baseUrl, `/channels/${channelId}/pinned-messages`),
      jsonRequestInit("GET", token),
    );
    if (!res.ok || !Array.isArray(res.body)) {
      throw new ApiError(res.status, extractErrorMessage(res.body) ?? kGenericError);
    }
    return res.body.map(toPinnedMessageInfo);
  }

  async uploadAttachment(
    token: string,
    channelId: number,
    filename: string,
    contentType: string,
    data: Uint8Array,
  ): Promise<{ id: number; filename: string }> {
    const res = await requestJson<{ id?: number; filename?: string }>(
      this.fetchImpl,
      resolveUrl(this.baseUrl, `/channels/${channelId}/attachments`),
      jsonRequestInit("POST", token, { filename, content_type: contentType, data_base64: toBase64(data) }),
    );
    if (!res.ok || res.body?.id === undefined || res.body.filename === undefined) {
      throw new ApiError(res.status, extractErrorMessage(res.body) ?? kGenericError);
    }
    return { id: res.body.id, filename: res.body.filename };
  }

  /** @returns the raw file bytes — unlike every other method here, the
   * success body isn't JSON at all (mirrors
   * ChatRestClient::downloadAttachment()), so only a non-2xx response is
   * parsed as JSON, to read its `error` field. */
  async downloadAttachment(token: string, attachmentId: number): Promise<Uint8Array> {
    const response = await this.fetchImpl(
      resolveUrl(this.baseUrl, `/attachments/${attachmentId}`),
      jsonRequestInit("GET", token),
    );
    if (!response.ok) {
      const body = await readJsonBody<{ error?: string }>(response);
      throw new ApiError(response.status, extractErrorMessage(body) ?? kGenericError);
    }
    return new Uint8Array(await response.arrayBuffer());
  }

  async searchMessages(
    token: string,
    channelId: number,
    query: string,
    limit = 20,
  ): Promise<ChatMessageInfo[]> {
    const params = new URLSearchParams({ q: query, limit: String(limit) });
    const res = await requestJson<MessageBody[]>(
      this.fetchImpl,
      resolveUrl(this.baseUrl, `/channels/${channelId}/messages/search?${params.toString()}`),
      jsonRequestInit("GET", token),
    );
    if (!res.ok || !Array.isArray(res.body)) {
      throw new ApiError(res.status, extractErrorMessage(res.body) ?? kGenericError);
    }
    return res.body.map(toChatMessageInfo);
  }

  // ---- Direct messages (issue #187) ----

  /** The response only carries `{id}` — @p recipientLogin is echoed back
   * from the argument (matching `dmThreadOpened(id, otherLogin)`, which
   * likewise never carries a `createdAt`). Idempotent: repeated calls
   * with the same recipient resolve to the same thread id. */
  async openDmThread(token: string, recipientLogin: string): Promise<{ id: number; otherLogin: string }> {
    const res = await requestJson<{ id?: number }>(
      this.fetchImpl,
      resolveUrl(this.baseUrl, "/dm/threads"),
      jsonRequestInit("POST", token, { recipient_login: recipientLogin }),
    );
    if (!res.ok || res.body?.id === undefined) {
      throw new ApiError(res.status, extractErrorMessage(res.body) ?? kGenericError);
    }
    return { id: res.body.id, otherLogin: recipientLogin };
  }

  async listDmThreads(token: string): Promise<DirectMessageThreadInfo[]> {
    const res = await requestJson<DirectMessageThreadBody[]>(
      this.fetchImpl,
      resolveUrl(this.baseUrl, "/dm/threads"),
      jsonRequestInit("GET", token),
    );
    if (!res.ok || !Array.isArray(res.body)) {
      throw new ApiError(res.status, extractErrorMessage(res.body) ?? kGenericError);
    }
    return res.body.map(toDirectMessageThreadInfo);
  }

  async sendDirectMessage(token: string, threadId: number, body: string): Promise<DirectMessageInfo> {
    const res = await requestJson<DirectMessageBody>(
      this.fetchImpl,
      resolveUrl(this.baseUrl, `/dm/threads/${threadId}/messages`),
      jsonRequestInit("POST", token, { body }),
    );
    if (!res.ok || res.body === undefined) {
      throw new ApiError(res.status, extractErrorMessage(res.body) ?? kGenericError);
    }
    return toDirectMessageInfo(res.body);
  }

  async listDirectMessages(
    token: string,
    threadId: number,
    limit: number,
    beforeId?: number,
  ): Promise<DirectMessageInfo[]> {
    const res = await requestJson<DirectMessageBody[]>(
      this.fetchImpl,
      resolveUrl(this.baseUrl, `/dm/threads/${threadId}/messages?${messagesQuery(limit, beforeId)}`),
      jsonRequestInit("GET", token),
    );
    if (!res.ok || !Array.isArray(res.body)) {
      throw new ApiError(res.status, extractErrorMessage(res.body) ?? kGenericError);
    }
    return res.body.map(toDirectMessageInfo);
  }

  // ---- Shared response-shape helpers ----

  private async expectOk(pending: Promise<{ status: number; ok: boolean; body: unknown }>): Promise<void> {
    const res = await pending;
    if (!res.ok) {
      throw new ApiError(res.status, extractErrorMessage(res.body) ?? kGenericError);
    }
  }

  private async expectItem(
    pending: Promise<{ status: number; ok: boolean; body: ChatItemBody | undefined }>,
  ): Promise<ChatItem> {
    const res = await pending;
    if (!res.ok || res.body === undefined) {
      throw new ApiError(res.status, extractErrorMessage(res.body) ?? kGenericError);
    }
    return toChatItem(res.body);
  }

  private async expectItemArray(
    pending: Promise<{ status: number; ok: boolean; body: ChatItemBody[] | undefined }>,
  ): Promise<ChatItem[]> {
    const res = await pending;
    if (!res.ok || !Array.isArray(res.body)) {
      throw new ApiError(res.status, extractErrorMessage(res.body) ?? kGenericError);
    }
    return res.body.map(toChatItem);
  }

  private async expectStringArray(
    pending: Promise<{ status: number; ok: boolean; body: string[] | undefined }>,
  ): Promise<string[]> {
    const res = await pending;
    if (!res.ok || !Array.isArray(res.body)) {
      throw new ApiError(res.status, extractErrorMessage(res.body) ?? kGenericError);
    }
    return res.body;
  }
}
