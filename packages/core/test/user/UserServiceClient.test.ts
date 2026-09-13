import { describe, expect, it, vi } from "vitest";

import { UserServiceClient } from "../../src/user/UserServiceClient.js";
import { ApiError } from "../../src/http.js";
import type { FetchLike } from "../../src/http.js";

function jsonResponse(status: number, body: unknown): Response {
  return new Response(JSON.stringify(body), { status });
}

function emptyResponse(status: number): Response {
  return new Response(null, { status });
}

function fakeFetch(response: Response): FetchLike {
  return vi.fn().mockResolvedValue(response);
}

const kBaseUrl = "https://user.example.test";
const kToken = "access-token";

describe("UserServiceClient", () => {
  describe("fetchProfile", () => {
    it("resolves with email/telegram_chat_id when viewing your own profile", async () => {
      const fetchImpl = fakeFetch(
        jsonResponse(200, {
          login: "alice",
          display_name: "Alice",
          avatar_url: null,
          public_key: "pk1",
          email: "alice@example.test",
          telegram_chat_id: "12345",
        }),
      );
      const client = new UserServiceClient(kBaseUrl, fetchImpl);

      const profile = await client.fetchProfile(kToken, "alice");

      expect(profile).toEqual({
        login: "alice",
        displayName: "Alice",
        publicKey: "pk1",
        email: "alice@example.test",
        telegramChatId: "12345",
      });
      expect(fetchImpl).toHaveBeenCalledWith(
        `${kBaseUrl}/users/alice/profile`,
        expect.objectContaining({
          method: "GET",
          headers: expect.objectContaining({ Authorization: `Bearer ${kToken}` }),
        }),
      );
    });

    it("issue #225/#243: omits email/telegram_chat_id entirely for someone else's profile", async () => {
      const fetchImpl = fakeFetch(
        jsonResponse(200, { login: "bob", display_name: "Bob", avatar_url: null, public_key: null }),
      );
      const client = new UserServiceClient(kBaseUrl, fetchImpl);

      const profile = await client.fetchProfile(kToken, "bob");

      expect(profile.email).toBeUndefined();
      expect(profile.telegramChatId).toBeUndefined();
      expect(profile).not.toHaveProperty("email");
      expect(profile).not.toHaveProperty("telegramChatId");
      expect(profile.login).toBe("bob");
      expect(profile.displayName).toBe("Bob");
    });

    it("percent-encodes the login in the URL path", async () => {
      const fetchImpl = fakeFetch(
        jsonResponse(200, { login: "a b", display_name: null, avatar_url: null, public_key: null }),
      );
      const client = new UserServiceClient(kBaseUrl, fetchImpl);

      await client.fetchProfile(kToken, "a b");

      expect(fetchImpl).toHaveBeenCalledWith(`${kBaseUrl}/users/a%20b/profile`, expect.anything());
    });

    it("rejects with the server's error message on 401", async () => {
      const fetchImpl = fakeFetch(jsonResponse(401, { error: "unauthorized" }));
      const client = new UserServiceClient(kBaseUrl, fetchImpl);

      await expect(client.fetchProfile(kToken, "alice")).rejects.toMatchObject({
        status: 401,
        message: "unauthorized",
      });
    });

    it("rejects on 404 for a nonexistent user", async () => {
      const fetchImpl = fakeFetch(jsonResponse(404, { error: "no such user" }));
      const client = new UserServiceClient(kBaseUrl, fetchImpl);

      await expect(client.fetchProfile(kToken, "ghost")).rejects.toMatchObject({
        status: 404,
        message: "no such user",
      });
    });
  });

  describe("updateOwnProfile", () => {
    it("sends all four fields and resolves with the updated profile", async () => {
      const fetchImpl = fakeFetch(
        jsonResponse(200, {
          login: "alice",
          display_name: "Alice B.",
          avatar_url: "https://example.test/a.png",
          public_key: null,
          email: "alice@example.test",
          telegram_chat_id: null,
        }),
      );
      const client = new UserServiceClient(kBaseUrl, fetchImpl);

      const profile = await client.updateOwnProfile(kToken, {
        displayName: "Alice B.",
        avatarUrl: "https://example.test/a.png",
        email: "alice@example.test",
        telegramChatId: "",
      });

      expect(profile.displayName).toBe("Alice B.");
      expect(fetchImpl).toHaveBeenCalledWith(
        `${kBaseUrl}/users/me`,
        expect.objectContaining({
          method: "PATCH",
          body: JSON.stringify({
            display_name: "Alice B.",
            avatar_url: "https://example.test/a.png",
            email: "alice@example.test",
            telegram_chat_id: "",
          }),
        }),
      );
    });

    it("rejects on a conflicting email (409)", async () => {
      const fetchImpl = fakeFetch(jsonResponse(409, { error: "email already in use" }));
      const client = new UserServiceClient(kBaseUrl, fetchImpl);

      await expect(
        client.updateOwnProfile(kToken, {
          displayName: "",
          avatarUrl: "",
          email: "taken@example.test",
          telegramChatId: "",
        }),
      ).rejects.toMatchObject({ status: 409, message: "email already in use" });
    });
  });

  describe("publishPublicKey", () => {
    it("sends only public_key, resolving with the updated profile", async () => {
      const fetchImpl = fakeFetch(
        jsonResponse(200, {
          login: "alice",
          display_name: null,
          avatar_url: null,
          public_key: "pk1",
          email: null,
          telegram_chat_id: null,
        }),
      );
      const client = new UserServiceClient(kBaseUrl, fetchImpl);

      const profile = await client.publishPublicKey(kToken, "pk1");

      expect(profile.publicKey).toBe("pk1");
      expect(fetchImpl).toHaveBeenCalledWith(
        `${kBaseUrl}/users/me`,
        expect.objectContaining({ method: "PATCH", body: JSON.stringify({ public_key: "pk1" }) }),
      );
    });
  });

  describe("uploadAvatar", () => {
    it("base64-encodes the bytes and resolves with the returned avatar_url", async () => {
      const fetchImpl = fakeFetch(jsonResponse(200, { avatar_url: "/users/alice/avatar" }));
      const client = new UserServiceClient(kBaseUrl, fetchImpl);

      const result = await client.uploadAvatar(kToken, "image/png", new Uint8Array([0x66, 0x6f, 0x6f]));

      expect(result).toEqual({ avatarUrl: "/users/alice/avatar" });
      expect(fetchImpl).toHaveBeenCalledWith(
        `${kBaseUrl}/profile/avatar`,
        expect.objectContaining({
          method: "POST",
          body: JSON.stringify({ content_type: "image/png", data_base64: "Zm9v" }),
        }),
      );
    });

    it("throws ApiError with the server's message on a rejected upload", async () => {
      const fetchImpl = fakeFetch(
        jsonResponse(400, { error: "'content_type' must be an image/* MIME type" }),
      );
      const client = new UserServiceClient(kBaseUrl, fetchImpl);

      await expect(client.uploadAvatar(kToken, "text/html", new Uint8Array([1]))).rejects.toThrow(
        "'content_type' must be an image/* MIME type",
      );
    });
  });

  describe("sendFriendRequest", () => {
    it("resolves 'sent' for a fresh request", async () => {
      const fetchImpl = fakeFetch(jsonResponse(201, { status: "sent" }));
      const client = new UserServiceClient(kBaseUrl, fetchImpl);

      await expect(client.sendFriendRequest(kToken, "bob")).resolves.toBe("sent");
    });

    it("resolves 'accepted' when the recipient already had a reciprocal request", async () => {
      const fetchImpl = fakeFetch(jsonResponse(201, { status: "accepted" }));
      const client = new UserServiceClient(kBaseUrl, fetchImpl);

      await expect(client.sendFriendRequest(kToken, "bob")).resolves.toBe("accepted");
    });

    it("rejects when trying to friend yourself (400)", async () => {
      const fetchImpl = fakeFetch(jsonResponse(400, { error: "cannot send a friend request to yourself" }));
      const client = new UserServiceClient(kBaseUrl, fetchImpl);

      await expect(client.sendFriendRequest(kToken, "alice")).rejects.toMatchObject({ status: 400 });
    });

    it("rejects on an already-pending request (409)", async () => {
      const fetchImpl = fakeFetch(jsonResponse(409, { error: "a pending request already exists" }));
      const client = new UserServiceClient(kBaseUrl, fetchImpl);

      await expect(client.sendFriendRequest(kToken, "bob")).rejects.toMatchObject({ status: 409 });
    });
  });

  describe("listIncomingFriendRequests", () => {
    it("resolves with the mapped list", async () => {
      const fetchImpl = fakeFetch(
        jsonResponse(200, [{ id: 1, requester_login: "bob", created_at: "2026-01-01T00:00:00Z" }]),
      );
      const client = new UserServiceClient(kBaseUrl, fetchImpl);

      await expect(client.listIncomingFriendRequests(kToken)).resolves.toEqual([
        { id: 1, requesterLogin: "bob", createdAt: "2026-01-01T00:00:00Z" },
      ]);
    });

    it("resolves with an empty array when there are none", async () => {
      const fetchImpl = fakeFetch(jsonResponse(200, []));
      const client = new UserServiceClient(kBaseUrl, fetchImpl);

      await expect(client.listIncomingFriendRequests(kToken)).resolves.toEqual([]);
    });
  });

  describe("acceptFriendRequest / declineFriendRequest", () => {
    it("resolves on a successful accept", async () => {
      const fetchImpl = fakeFetch(emptyResponse(200));
      const client = new UserServiceClient(kBaseUrl, fetchImpl);

      await expect(client.acceptFriendRequest(kToken, 42)).resolves.toBeUndefined();
      expect(fetchImpl).toHaveBeenCalledWith(
        `${kBaseUrl}/friends/requests/42/accept`,
        expect.objectContaining({ method: "POST" }),
      );
    });

    it("resolves on a successful decline", async () => {
      const fetchImpl = fakeFetch(emptyResponse(200));
      const client = new UserServiceClient(kBaseUrl, fetchImpl);

      await expect(client.declineFriendRequest(kToken, 42)).resolves.toBeUndefined();
      expect(fetchImpl).toHaveBeenCalledWith(
        `${kBaseUrl}/friends/requests/42/decline`,
        expect.objectContaining({ method: "POST" }),
      );
    });

    it("issue #225 (pentest): rejects with 404 for someone else's request, not your own pending one", async () => {
      const fetchImpl = fakeFetch(jsonResponse(404, { error: "no such pending request" }));
      const client = new UserServiceClient(kBaseUrl, fetchImpl);

      await expect(client.acceptFriendRequest(kToken, 999)).rejects.toMatchObject({ status: 404 });
    });
  });

  describe("listFriends", () => {
    it("resolves with the login list", async () => {
      const fetchImpl = fakeFetch(jsonResponse(200, ["bob", "carol"]));
      const client = new UserServiceClient(kBaseUrl, fetchImpl);

      await expect(client.listFriends(kToken)).resolves.toEqual(["bob", "carol"]);
    });
  });

  describe("removeFriend", () => {
    it("percent-encodes the login and resolves on success", async () => {
      const fetchImpl = fakeFetch(emptyResponse(200));
      const client = new UserServiceClient(kBaseUrl, fetchImpl);

      await expect(client.removeFriend(kToken, "bob smith")).resolves.toBeUndefined();
      expect(fetchImpl).toHaveBeenCalledWith(
        `${kBaseUrl}/friends/bob%20smith`,
        expect.objectContaining({ method: "DELETE" }),
      );
    });

    it("rejects with 404 when not friends", async () => {
      const fetchImpl = fakeFetch(jsonResponse(404, { error: "not friends" }));
      const client = new UserServiceClient(kBaseUrl, fetchImpl);

      await expect(client.removeFriend(kToken, "stranger")).rejects.toMatchObject({
        status: 404,
        message: "not friends",
      });
    });
  });

  it("uses ApiError as the rejection type", async () => {
    const fetchImpl = fakeFetch(jsonResponse(401, { error: "unauthorized" }));
    const client = new UserServiceClient(kBaseUrl, fetchImpl);

    await expect(client.fetchProfile(kToken, "alice")).rejects.toBeInstanceOf(ApiError);
  });
});
