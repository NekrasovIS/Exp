import { describe, expect, it } from "vitest";

import { findFirstUrl } from "../../src/chat/findFirstUrl.js";

describe("findFirstUrl", () => {
  it("returns null when there's no URL", () => {
    expect(findFirstUrl("just some plain text")).toBeNull();
  });

  it("finds an https URL embedded in a sentence", () => {
    expect(findFirstUrl("check this out: https://example.test/article")).toBe("https://example.test/article");
  });

  it("finds an http URL", () => {
    expect(findFirstUrl("http://example.test/")).toBe("http://example.test/");
  });

  it("returns only the first URL when there are several", () => {
    expect(findFirstUrl("https://a.test/ and https://b.test/")).toBe("https://a.test/");
  });

  it("stops at whitespace", () => {
    expect(findFirstUrl("https://example.test/path first then more text")).toBe("https://example.test/path");
  });

  it("does not match a bare domain without a scheme", () => {
    expect(findFirstUrl("visit example.test today")).toBeNull();
  });
});
