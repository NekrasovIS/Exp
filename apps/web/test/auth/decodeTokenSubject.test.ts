import { describe, expect, it } from "vitest";

import { decodeTokenSubject } from "../../src/auth/decodeTokenSubject.js";

function encodePayloadBase64Url(payload: unknown): string {
  const base64 = btoa(JSON.stringify(payload));
  return base64.replaceAll("+", "-").replaceAll("/", "_").replace(/=+$/, "");
}

describe("decodeTokenSubject", () => {
  it("reads the subject from a well-formed token payload", () => {
    const token = `${encodePayloadBase64Url({ sub: "alice", exp: 9999999999 })}.signature-not-checked-here`;
    expect(decodeTokenSubject(token)).toBe("alice");
  });

  it("handles a payload segment whose base64url length needs padding", () => {
    // "alice-with-a-longer-login" pushes the JSON payload length such
    // that its base64 needs '=' padding restored before atob() accepts it.
    const token = `${encodePayloadBase64Url({ sub: "alice-with-a-longer-login", exp: 1 })}.sig`;
    expect(decodeTokenSubject(token)).toBe("alice-with-a-longer-login");
  });

  it("returns null for a malformed token", () => {
    expect(decodeTokenSubject("not-base64.sig")).toBeNull();
  });

  it("returns null when the payload has no 'sub' field", () => {
    const token = `${encodePayloadBase64Url({ exp: 1 })}.sig`;
    expect(decodeTokenSubject(token)).toBeNull();
  });
});
