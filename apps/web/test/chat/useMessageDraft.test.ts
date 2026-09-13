import { act, renderHook } from "@testing-library/react";
import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";

import { useMessageDraft } from "../../src/chat/useMessageDraft.js";

beforeEach(() => {
  vi.useFakeTimers();
});

afterEach(() => {
  vi.useRealTimers();
  localStorage.clear();
});

describe("useMessageDraft", () => {
  it("starts empty when nothing is stored for this key", () => {
    const { result } = renderHook(() => useMessageDraft("channel:1"));
    expect(result.current.draft).toBe("");
  });

  it("debounces the localStorage write instead of writing on every call", () => {
    const { result } = renderHook(() => useMessageDraft("channel:1"));

    act(() => result.current.setDraft("h"));
    act(() => result.current.setDraft("he"));
    act(() => result.current.setDraft("hey"));
    expect(localStorage.getItem("devicehub.web.draft.channel:1")).toBeNull();

    act(() => vi.advanceTimersByTime(500));
    expect(localStorage.getItem("devicehub.web.draft.channel:1")).toBe("hey");
  });

  it("restores a previously saved draft for the same key on mount", () => {
    localStorage.setItem("devicehub.web.draft.channel:1", "unsent text");
    const { result } = renderHook(() => useMessageDraft("channel:1"));
    expect(result.current.draft).toBe("unsent text");
  });

  it("switching to a different key loads that key's own draft", () => {
    localStorage.setItem("devicehub.web.draft.channel:2", "other channel's draft");
    const { result, rerender } = renderHook(({ key }) => useMessageDraft(key), {
      initialProps: { key: "channel:1" },
    });
    expect(result.current.draft).toBe("");

    rerender({ key: "channel:2" });
    expect(result.current.draft).toBe("other channel's draft");
  });

  it("clearDraft empties the state and removes the stored value immediately", () => {
    const { result } = renderHook(() => useMessageDraft("channel:1"));

    act(() => result.current.setDraft("hey"));
    act(() => vi.advanceTimersByTime(500));
    expect(localStorage.getItem("devicehub.web.draft.channel:1")).toBe("hey");

    act(() => result.current.clearDraft());
    expect(result.current.draft).toBe("");
    expect(localStorage.getItem("devicehub.web.draft.channel:1")).toBeNull();
  });

  it("clearDraft cancels a still-pending debounced write for the same key", () => {
    const { result } = renderHook(() => useMessageDraft("channel:1"));

    act(() => result.current.setDraft("hey"));
    act(() => result.current.clearDraft());
    act(() => vi.advanceTimersByTime(500));

    expect(localStorage.getItem("devicehub.web.draft.channel:1")).toBeNull();
  });
});
