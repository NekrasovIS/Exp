import { describe, expect, it } from "vitest";

import { extractErrorMessage, resolveUrl } from "../src/http.js";

describe("resolveUrl", () => {
  it("joins a base URL without a trailing slash", () => {
    expect(resolveUrl("https://api.example.test", "/auth/token")).toBe("https://api.example.test/auth/token");
  });

  it("strips a trailing slash from the base URL before joining", () => {
    expect(resolveUrl("https://api.example.test/", "/auth/token")).toBe(
      "https://api.example.test/auth/token",
    );
  });
});

describe("extractErrorMessage", () => {
  it("returns the 'error' field when present and a string", () => {
    expect(extractErrorMessage({ error: "invalid credentials" })).toBe("invalid credentials");
  });

  it("returns undefined when the field is missing", () => {
    expect(extractErrorMessage({ other: "field" })).toBeUndefined();
  });

  it("returns undefined when the 'error' field isn't a string", () => {
    expect(extractErrorMessage({ error: 42 })).toBeUndefined();
  });

  it("returns undefined for non-object bodies (null, primitives, undefined)", () => {
    expect(extractErrorMessage(null)).toBeUndefined();
    expect(extractErrorMessage(undefined)).toBeUndefined();
    expect(extractErrorMessage("plain string")).toBeUndefined();
  });
});
