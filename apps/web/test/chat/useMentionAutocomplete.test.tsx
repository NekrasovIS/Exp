import { act, renderHook, waitFor } from "@testing-library/react";
import React from "react";
import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";

import { useMentionAutocomplete } from "../../src/chat/useMentionAutocomplete.js";
import { SessionProvider } from "../../src/session/SessionContext.js";
import { jsonResponse } from "../testUtils.js";

const kStorageKey = "devicehub.web.session";

function Wrapper({ children }: { children: React.ReactNode }) {
  return <SessionProvider>{children}</SessionProvider>;
}

beforeEach(() => {
  localStorage.setItem(
    kStorageKey,
    JSON.stringify({
      token: "access-token",
      refreshToken: "refresh-token",
      expiresAt: Math.floor(Date.now() / 1000) + 3600,
    }),
  );
});

afterEach(() => {
  vi.unstubAllGlobals();
  localStorage.clear();
});

function renderReady() {
  vi.stubGlobal("fetch", vi.fn().mockResolvedValue(jsonResponse(200, ["alice", "bob", "carol"])));
  return renderHook(() => useMentionAutocomplete(1), { wrapper: Wrapper }).result;
}

// The member list loads asynchronously (a REST fetch on mount) — each
// test drives handleTextChange() inside waitFor() so it naturally
// retries until that fetch has resolved and memberLogins is populated,
// rather than needing a separately-exposed "loaded" flag from the hook.
async function waitForSuggestions(
  result: ReturnType<typeof renderReady>,
  text: string,
  cursorPos: number,
): Promise<void> {
  await waitFor(() => {
    act(() => result.current.handleTextChange(text, cursorPos));
    expect(result.current.suggestions.length).toBeGreaterThan(0);
  });
}

describe("useMentionAutocomplete", () => {
  it("filters loaded members by the typed prefix, case-insensitively", async () => {
    const result = renderReady();

    await waitForSuggestions(result, "hey @AL", 7);

    expect(result.current.suggestions).toEqual(["alice"]);
  });

  it("suggests every member when just '@' was typed", async () => {
    const result = renderReady();

    await waitForSuggestions(result, "hey @", 5);

    expect(result.current.suggestions).toEqual(["alice", "bob", "carol"]);
  });

  it("does not trigger without an '@' at all", async () => {
    const result = renderReady();
    // Establish the member list is actually loaded first (via a
    // triggering call), then confirm a non-triggering one yields no
    // suggestions — otherwise "no suggestions" would be indistinguishable
    // from "the fetch just hasn't resolved yet".
    await waitForSuggestions(result, "hey @", 5);

    act(() => result.current.handleTextChange("hello there", 11));

    expect(result.current.suggestions).toEqual([]);
  });

  it("does not treat an email-like '@' (not at a word boundary) as a trigger", async () => {
    const result = renderReady();
    await waitForSuggestions(result, "hey @", 5);

    act(() => result.current.handleTextChange("contact user@example", 21));

    expect(result.current.suggestions).toEqual([]);
  });

  it("stops suggesting once the typed prefix contains a space", async () => {
    const result = renderReady();
    await waitForSuggestions(result, "hey @", 5);

    act(() => result.current.handleTextChange("hey @al ready", 8));

    expect(result.current.suggestions).toEqual([]);
  });

  it("moveActive cycles forward and backward, wrapping at the ends", async () => {
    const result = renderReady();
    await waitForSuggestions(result, "hey @", 5);
    expect(result.current.activeIndex).toBe(0);

    act(() => result.current.moveActive(1));
    expect(result.current.activeIndex).toBe(1);

    act(() => result.current.moveActive(-1));
    act(() => result.current.moveActive(-1));
    expect(result.current.activeIndex).toBe(2); // wrapped from 0 to the last index
  });

  it("applySuggestion inserts '@login ' at the trigger position and preserves the tail", async () => {
    const result = renderReady();
    await waitForSuggestions(result, "hey @al, welcome", 7);

    let applied;
    act(() => {
      applied = result.current.applySuggestion("hey @al, welcome", 7, "alice");
    });

    expect(applied).toEqual({ text: "hey @alice , welcome", cursorPos: 11 });
  });

  it("applyActive uses the currently highlighted suggestion", async () => {
    const result = renderReady();
    await waitForSuggestions(result, "hey @", 5);
    act(() => result.current.moveActive(1)); // now on "bob"

    let applied;
    act(() => {
      applied = result.current.applyActive("hey @", 5);
    });

    expect(applied).toEqual({ text: "hey @bob ", cursorPos: 9 });
  });

  it("dismiss clears the suggestions without changing any text", async () => {
    const result = renderReady();
    await waitForSuggestions(result, "hey @al", 7);

    act(() => result.current.dismiss());

    expect(result.current.suggestions).toEqual([]);
  });
});
