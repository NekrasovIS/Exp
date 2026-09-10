import { describe, expect, it, vi } from "vitest";

import { ChatRestClient } from "../../src/chat/ChatRestClient.js";
import { ApiError } from "../../src/http.js";
import type { FetchLike } from "../../src/http.js";

function jsonResponse(status: number, body: unknown): Response {
  return new Response(JSON.stringify(body), { status });
}

function rawResponse(status: number, bytes: Uint8Array): Response {
  return new Response(bytes, { status });
}

function fakeFetch(response: Response): FetchLike {
  return vi.fn().mockResolvedValue(response);
}

const kBaseUrl = "https://chat.example.test";
const kToken = "access-token";

describe("ChatRestClient", () => {
  describe("communities", () => {
    it("createCommunity resolves with the created community", async () => {
      const fetchImpl = fakeFetch(
        jsonResponse(200, { id: 1, name: "Alpha", owner: "alice", invite_code: "ABC123" }),
      );
      const client = new ChatRestClient(kBaseUrl, fetchImpl);

      await expect(client.createCommunity(kToken, "Alpha")).resolves.toEqual({
        id: 1,
        name: "Alpha",
        ownerLogin: "alice",
        isEncrypted: false,
        inviteCode: "ABC123",
      });
      expect(fetchImpl).toHaveBeenCalledWith(
        `${kBaseUrl}/communities`,
        expect.objectContaining({ method: "POST", body: JSON.stringify({ name: "Alpha" }) }),
      );
    });

    it("listCommunities resolves with the mapped list from /communities/mine", async () => {
      const fetchImpl = fakeFetch(jsonResponse(200, [{ id: 1, name: "Alpha", owner: "alice" }]));
      const client = new ChatRestClient(kBaseUrl, fetchImpl);

      await expect(client.listCommunities(kToken)).resolves.toEqual([
        { id: 1, name: "Alpha", ownerLogin: "alice", isEncrypted: false },
      ]);
      expect(fetchImpl).toHaveBeenCalledWith(`${kBaseUrl}/communities/mine`, expect.anything());
    });

    it("joinCommunityByCode rejects with 'invalid invite code' for an unknown code", async () => {
      const fetchImpl = fakeFetch(jsonResponse(404, { error: "invalid invite code" }));
      const client = new ChatRestClient(kBaseUrl, fetchImpl);

      await expect(client.joinCommunityByCode(kToken, "NOPE")).rejects.toMatchObject({
        status: 404,
        message: "invalid invite code",
      });
    });

    it("regenerateInviteCode resolves with the new code", async () => {
      const fetchImpl = fakeFetch(jsonResponse(200, { invite_code: "NEWCODE" }));
      const client = new ChatRestClient(kBaseUrl, fetchImpl);

      await expect(client.regenerateInviteCode(kToken, 1)).resolves.toBe("NEWCODE");
    });
  });

  describe("channels", () => {
    it("createChannel echoes back the given name, taking is_encrypted from the response", async () => {
      const fetchImpl = fakeFetch(jsonResponse(200, { id: 5, is_encrypted: true }));
      const client = new ChatRestClient(kBaseUrl, fetchImpl);

      await expect(client.createChannel(kToken, 1, "secret", true)).resolves.toEqual({
        id: 5,
        name: "secret",
        isEncrypted: true,
      });
    });

    it("listMembers resolves with a plain login array", async () => {
      const fetchImpl = fakeFetch(jsonResponse(200, ["alice", "bob"]));
      const client = new ChatRestClient(kBaseUrl, fetchImpl);

      await expect(client.listMembers(kToken, 1)).resolves.toEqual(["alice", "bob"]);
    });
  });

  describe("fetchMyChannelKey", () => {
    it("resolves with the wrapped key when one exists", async () => {
      const fetchImpl = fakeFetch(jsonResponse(200, { wrapped_key: "sealed-bytes" }));
      const client = new ChatRestClient(kBaseUrl, fetchImpl);

      await expect(client.fetchMyChannelKey(kToken, 1)).resolves.toBe("sealed-bytes");
    });

    it("resolves with null on 404, instead of rejecting (a member with no key yet is expected, not an error)", async () => {
      const fetchImpl = fakeFetch(jsonResponse(404, { error: "not found" }));
      const client = new ChatRestClient(kBaseUrl, fetchImpl);

      await expect(client.fetchMyChannelKey(kToken, 1)).resolves.toBeNull();
    });

    it("still rejects for a real error (e.g. 401)", async () => {
      const fetchImpl = fakeFetch(jsonResponse(401, { error: "unauthorized" }));
      const client = new ChatRestClient(kBaseUrl, fetchImpl);

      await expect(client.fetchMyChannelKey(kToken, 1)).rejects.toMatchObject({ status: 401 });
    });
  });

  describe("setChannelKey", () => {
    it("percent-encodes the member login in the path", async () => {
      const fetchImpl = fakeFetch(new Response(null, { status: 200 }));
      const client = new ChatRestClient(kBaseUrl, fetchImpl);

      await client.setChannelKey(kToken, 1, "a b", "wrapped");

      expect(fetchImpl).toHaveBeenCalledWith(
        `${kBaseUrl}/channels/1/keys/a%20b`,
        expect.objectContaining({ method: "PUT", body: JSON.stringify({ wrapped_key: "wrapped" }) }),
      );
    });
  });

  describe("messages", () => {
    it("listMessages omits before_id when not given", async () => {
      const fetchImpl = fakeFetch(jsonResponse(200, []));
      const client = new ChatRestClient(kBaseUrl, fetchImpl);

      await client.listMessages(kToken, 1, 20);

      expect(fetchImpl).toHaveBeenCalledWith(`${kBaseUrl}/channels/1/messages?limit=20`, expect.anything());
    });

    it("listMessages includes before_id when given and non-negative", async () => {
      const fetchImpl = fakeFetch(jsonResponse(200, []));
      const client = new ChatRestClient(kBaseUrl, fetchImpl);

      await client.listMessages(kToken, 1, 20, 99);

      expect(fetchImpl).toHaveBeenCalledWith(
        `${kBaseUrl}/channels/1/messages?limit=20&before_id=99`,
        expect.anything(),
      );
    });

    it("listMessages resolves with mapped messages, converting a missing attachment to undefined", async () => {
      const fetchImpl = fakeFetch(
        jsonResponse(200, [
          { id: 1, author: "alice", body: "hi", sent_at: "2026-01-01T00:00:00Z", attachment_id: null },
        ]),
      );
      const client = new ChatRestClient(kBaseUrl, fetchImpl);

      const messages = await client.listMessages(kToken, 1, 20);

      expect(messages[0]?.attachmentId).toBeUndefined();
      expect(messages[0]).not.toHaveProperty("attachmentFilename");
    });

    it("fetchLatestMessage silently resolves to [] on error instead of rejecting", async () => {
      const fetchImpl = fakeFetch(jsonResponse(500, { error: "boom" }));
      const client = new ChatRestClient(kBaseUrl, fetchImpl);

      await expect(client.fetchLatestMessage(kToken, 1)).resolves.toEqual([]);
    });

    it("fetchLatestMessage silently resolves to [] when fetch itself throws", async () => {
      const fetchImpl: FetchLike = vi.fn().mockRejectedValue(new TypeError("network down"));
      const client = new ChatRestClient(kBaseUrl, fetchImpl);

      await expect(client.fetchLatestMessage(kToken, 1)).resolves.toEqual([]);
    });

    it("searchMessages sends q and limit as query params", async () => {
      const fetchImpl = fakeFetch(jsonResponse(200, []));
      const client = new ChatRestClient(kBaseUrl, fetchImpl);

      await client.searchMessages(kToken, 1, "hello world");

      expect(fetchImpl).toHaveBeenCalledWith(
        `${kBaseUrl}/channels/1/messages/search?q=hello+world&limit=20`,
        expect.anything(),
      );
    });
  });

  describe("attachments", () => {
    it("uploadAttachment base64-encodes the given bytes", async () => {
      const fetchImpl = fakeFetch(jsonResponse(200, { id: 1, filename: "a.txt" }));
      const client = new ChatRestClient(kBaseUrl, fetchImpl);

      await client.uploadAttachment(kToken, 1, "a.txt", "text/plain", new TextEncoder().encode("hi"));

      expect(fetchImpl).toHaveBeenCalledWith(
        `${kBaseUrl}/channels/1/attachments`,
        expect.objectContaining({
          body: JSON.stringify({ filename: "a.txt", content_type: "text/plain", data_base64: "aGk=" }),
        }),
      );
    });

    it("downloadAttachment resolves with raw bytes, not JSON", async () => {
      const bytes = new TextEncoder().encode("file contents");
      const fetchImpl = fakeFetch(rawResponse(200, bytes));
      const client = new ChatRestClient(kBaseUrl, fetchImpl);

      const downloaded = await client.downloadAttachment(kToken, 1);

      expect(new TextDecoder().decode(downloaded)).toBe("file contents");
    });

    it("downloadAttachment rejects with the parsed error body on failure", async () => {
      const fetchImpl = fakeFetch(jsonResponse(404, { error: "not found" }));
      const client = new ChatRestClient(kBaseUrl, fetchImpl);

      await expect(client.downloadAttachment(kToken, 999)).rejects.toMatchObject({
        status: 404,
        message: "not found",
      });
    });
  });

  describe("direct messages", () => {
    it("openDmThread echoes back the recipient login", async () => {
      const fetchImpl = fakeFetch(jsonResponse(200, { id: 7 }));
      const client = new ChatRestClient(kBaseUrl, fetchImpl);

      await expect(client.openDmThread(kToken, "bob")).resolves.toEqual({ id: 7, otherLogin: "bob" });
    });

    it("openDmThread rejects with 403 when the recipient isn't a friend", async () => {
      const fetchImpl = fakeFetch(jsonResponse(403, { error: "can only message friends" }));
      const client = new ChatRestClient(kBaseUrl, fetchImpl);

      await expect(client.openDmThread(kToken, "stranger")).rejects.toMatchObject({
        status: 403,
        message: "can only message friends",
      });
    });

    it("sendDirectMessage resolves with the created message", async () => {
      const fetchImpl = fakeFetch(
        jsonResponse(200, { id: 1, author: "alice", body: "hi", sent_at: "2026-01-01T00:00:00Z" }),
      );
      const client = new ChatRestClient(kBaseUrl, fetchImpl);

      await expect(client.sendDirectMessage(kToken, 7, "hi")).resolves.toEqual({
        id: 1,
        author: "alice",
        body: "hi",
        sentAt: "2026-01-01T00:00:00Z",
      });
    });

    it("listDirectMessages resolves with the mapped thread history", async () => {
      const fetchImpl = fakeFetch(
        jsonResponse(200, [{ id: 1, author: "alice", body: "hi", sent_at: "2026-01-01T00:00:00Z" }]),
      );
      const client = new ChatRestClient(kBaseUrl, fetchImpl);

      await expect(client.listDirectMessages(kToken, 7, 20)).resolves.toEqual([
        { id: 1, author: "alice", body: "hi", sentAt: "2026-01-01T00:00:00Z" },
      ]);
    });
  });

  describe("unread counters", () => {
    it("markChannelRead posts message_id to /channels/{id}/read", async () => {
      const fetchImpl = fakeFetch(jsonResponse(200, {}));
      const client = new ChatRestClient(kBaseUrl, fetchImpl);

      await client.markChannelRead(kToken, 10, 42);

      expect(fetchImpl).toHaveBeenCalledWith(
        `${kBaseUrl}/channels/10/read`,
        expect.objectContaining({ method: "POST", body: JSON.stringify({ message_id: 42 }) }),
      );
    });

    it("markDmThreadRead posts message_id to /dm/threads/{id}/read", async () => {
      const fetchImpl = fakeFetch(jsonResponse(200, {}));
      const client = new ChatRestClient(kBaseUrl, fetchImpl);

      await client.markDmThreadRead(kToken, 7, 99);

      expect(fetchImpl).toHaveBeenCalledWith(
        `${kBaseUrl}/dm/threads/7/read`,
        expect.objectContaining({ method: "POST", body: JSON.stringify({ message_id: 99 }) }),
      );
    });

    it("fetchUnreadCounts maps both channels and dm_threads from /unread", async () => {
      const fetchImpl = fakeFetch(
        jsonResponse(200, {
          channels: [{ channel_id: 10, unread_count: 3 }],
          dm_threads: [{ thread_id: 7, unread_count: 1 }],
        }),
      );
      const client = new ChatRestClient(kBaseUrl, fetchImpl);

      await expect(client.fetchUnreadCounts(kToken)).resolves.toEqual({
        channels: [{ channelId: 10, unreadCount: 3 }],
        threads: [{ threadId: 7, unreadCount: 1 }],
      });
      expect(fetchImpl).toHaveBeenCalledWith(`${kBaseUrl}/unread`, expect.anything());
    });

    it("fetchUnreadCounts resolves with empty arrays when the server omits both fields", async () => {
      const fetchImpl = fakeFetch(jsonResponse(200, {}));
      const client = new ChatRestClient(kBaseUrl, fetchImpl);

      await expect(client.fetchUnreadCounts(kToken)).resolves.toEqual({ channels: [], threads: [] });
    });
  });

  it("uses ApiError as the rejection type", async () => {
    const fetchImpl = fakeFetch(jsonResponse(401, { error: "unauthorized" }));
    const client = new ChatRestClient(kBaseUrl, fetchImpl);

    await expect(client.listCommunities(kToken)).rejects.toBeInstanceOf(ApiError);
  });
});
