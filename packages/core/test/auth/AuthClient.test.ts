import { describe, expect, it, vi } from "vitest";

import { AuthClient } from "../../src/auth/AuthClient.js";
import { ApiError } from "../../src/http.js";
import type { FetchLike } from "../../src/http.js";

function jsonResponse(status: number, body: unknown): Response {
  return new Response(JSON.stringify(body), { status });
}

function fakeFetch(response: Response): FetchLike {
  return vi.fn().mockResolvedValue(response);
}

const kBaseUrl = "https://auth.example.test";

describe("AuthClient", () => {
  describe("requestToken", () => {
    it("resolves with the issued tokens on success", async () => {
      const fetchImpl = fakeFetch(jsonResponse(200, { token: "t1", refresh_token: "r1", expires_at: 1000 }));
      const client = new AuthClient(kBaseUrl, fetchImpl);

      const tokens = await client.requestToken("alice", "hunter2");

      expect(tokens).toEqual({ token: "t1", refreshToken: "r1", expiresAt: 1000 });
      expect(fetchImpl).toHaveBeenCalledWith(
        `${kBaseUrl}/auth/token`,
        expect.objectContaining({
          method: "POST",
          body: JSON.stringify({ login: "alice", password: "hunter2" }),
        }),
      );
    });

    it("rejects with the server's error message on invalid credentials", async () => {
      const fetchImpl = fakeFetch(jsonResponse(401, { error: "invalid credentials" }));
      const client = new AuthClient(kBaseUrl, fetchImpl);

      await expect(client.requestToken("alice", "wrong")).rejects.toMatchObject({
        status: 401,
        message: "invalid credentials",
      });
    });

    it("rejects with a generic message when the success body is malformed", async () => {
      const fetchImpl = fakeFetch(jsonResponse(200, { unexpected: true }));
      const client = new AuthClient(kBaseUrl, fetchImpl);

      await expect(client.requestToken("alice", "hunter2")).rejects.toThrow(
        "Malformed response from auth-service",
      );
    });
  });

  describe("verifyToken", () => {
    it("resolves with valid=true and the subject for a good token", async () => {
      const fetchImpl = fakeFetch(jsonResponse(200, { valid: true, subject: "alice" }));
      const client = new AuthClient(kBaseUrl, fetchImpl);

      await expect(client.verifyToken("t1")).resolves.toEqual({ valid: true, subject: "alice" });
    });

    it("resolves with valid=false for a bad token, not a rejection", async () => {
      const fetchImpl = fakeFetch(jsonResponse(200, { valid: false }));
      const client = new AuthClient(kBaseUrl, fetchImpl);

      await expect(client.verifyToken("garbage")).resolves.toEqual({ valid: false });
    });

    it("always uses the generic malformed message, never the body's own error field", async () => {
      // Mirrors AuthClient.cpp's asymmetry: unlike every other method,
      // a missing "valid" field never surfaces the body's "error" text.
      const fetchImpl = fakeFetch(jsonResponse(400, { error: "missing 'token' field" }));
      const client = new AuthClient(kBaseUrl, fetchImpl);

      await expect(client.verifyToken("")).rejects.toThrow("Malformed response from auth-service");
    });
  });

  describe("register", () => {
    it("auto-logs in on success (201, registered + tokens)", async () => {
      const fetchImpl = fakeFetch(
        jsonResponse(201, { registered: true, token: "t1", refresh_token: "r1", expires_at: 1000 }),
      );
      const client = new AuthClient(kBaseUrl, fetchImpl);

      const result = await client.register("alice", "hunter2");

      expect(result).toEqual({
        registered: true,
        tokens: { token: "t1", refreshToken: "r1", expiresAt: 1000 },
      });
    });

    it("resolves registered=false without tokens on a duplicate login (409)", async () => {
      const fetchImpl = fakeFetch(jsonResponse(409, { registered: false }));
      const client = new AuthClient(kBaseUrl, fetchImpl);

      const result = await client.register("alice", "hunter2");

      expect(result).toEqual({ registered: false });
      expect(result.tokens).toBeUndefined();
    });

    it("rejects when the body has no 'registered' field at all", async () => {
      const fetchImpl = fakeFetch(jsonResponse(500, {}));
      const client = new AuthClient(kBaseUrl, fetchImpl);

      await expect(client.register("alice", "hunter2")).rejects.toThrow(
        "Malformed response from auth-service",
      );
    });
  });

  describe("refreshAccessToken", () => {
    it("echoes the same refresh token back unchanged (no rotation)", async () => {
      const fetchImpl = fakeFetch(jsonResponse(200, { token: "t2", expires_at: 2000 }));
      const client = new AuthClient(kBaseUrl, fetchImpl);

      const tokens = await client.refreshAccessToken("r1");

      expect(tokens).toEqual({ token: "t2", refreshToken: "r1", expiresAt: 2000 });
    });

    it("rejects with the server's error message for an invalid/expired refresh token", async () => {
      const fetchImpl = fakeFetch(jsonResponse(401, { error: "invalid or expired refresh token" }));
      const client = new AuthClient(kBaseUrl, fetchImpl);

      await expect(client.refreshAccessToken("stale")).rejects.toMatchObject({
        status: 401,
        message: "invalid or expired refresh token",
      });
    });
  });

  describe("requestOtp", () => {
    it("resolves regardless of whether the identifier maps to a real account", async () => {
      // The server always answers 200 {sent: true} by design (issue
      // #156) — it never leaks account existence through this response.
      const fetchImpl = fakeFetch(jsonResponse(200, { sent: true }));
      const client = new AuthClient(kBaseUrl, fetchImpl);

      await expect(client.requestOtp("no-such-identifier@example.test")).resolves.toBeUndefined();
    });

    it("rejects on a malformed request (400)", async () => {
      const fetchImpl = fakeFetch(jsonResponse(400, { error: "expected an 'identifier' string" }));
      const client = new AuthClient(kBaseUrl, fetchImpl);

      await expect(client.requestOtp("")).rejects.toMatchObject({ status: 400 });
    });
  });

  describe("verifyOtp", () => {
    it("resolves with tokens on a correct code", async () => {
      const fetchImpl = fakeFetch(jsonResponse(200, { token: "t1", refresh_token: "r1", expires_at: 1000 }));
      const client = new AuthClient(kBaseUrl, fetchImpl);

      await expect(client.verifyOtp("alice", "123456")).resolves.toEqual({
        token: "t1",
        refreshToken: "r1",
        expiresAt: 1000,
      });
    });

    it("rejects on a wrong or expired code (401)", async () => {
      const fetchImpl = fakeFetch(jsonResponse(401, { error: "invalid or expired code" }));
      const client = new AuthClient(kBaseUrl, fetchImpl);

      await expect(client.verifyOtp("alice", "000000")).rejects.toMatchObject({
        status: 401,
        message: "invalid or expired code",
      });
    });
  });

  it("uses ApiError as the rejection type", async () => {
    const fetchImpl = fakeFetch(jsonResponse(401, { error: "invalid credentials" }));
    const client = new AuthClient(kBaseUrl, fetchImpl);

    await expect(client.requestToken("alice", "wrong")).rejects.toBeInstanceOf(ApiError);
  });
});
