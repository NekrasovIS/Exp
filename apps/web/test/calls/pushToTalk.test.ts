import { act, renderHook } from "@testing-library/react";
import { afterEach, describe, expect, it, vi } from "vitest";

import { usePushToTalk, usePushToTalkPreference } from "../../src/calls/pushToTalk.js";

afterEach(() => {
  localStorage.clear();
});

function fireKey(type: "keydown" | "keyup", key: string): void {
  document.dispatchEvent(new KeyboardEvent(type, { key }));
}

describe("usePushToTalkPreference", () => {
  it("starts disabled when nothing is stored", () => {
    const { result } = renderHook(() => usePushToTalkPreference());
    expect(result.current.enabled).toBe(false);
  });

  it("setEnabled(true) persists and is reflected immediately", () => {
    const { result } = renderHook(() => usePushToTalkPreference());

    act(() => result.current.setEnabled(true));

    expect(result.current.enabled).toBe(true);
    const { result: second } = renderHook(() => usePushToTalkPreference());
    expect(second.current.enabled).toBe(true);
  });
});

describe("usePushToTalk", () => {
  it("does nothing while inactive", () => {
    const setMuted = vi.fn();
    renderHook(() => usePushToTalk(false, setMuted));

    expect(setMuted).not.toHaveBeenCalled();
  });

  it("mutes immediately once active", () => {
    const setMuted = vi.fn();
    renderHook(() => usePushToTalk(true, setMuted));

    expect(setMuted).toHaveBeenCalledWith(true);
  });

  it("holding Control unmutes, releasing it mutes again", () => {
    const setMuted = vi.fn();
    renderHook(() => usePushToTalk(true, setMuted));
    setMuted.mockClear();

    act(() => fireKey("keydown", "Control"));
    expect(setMuted).toHaveBeenLastCalledWith(false);

    act(() => fireKey("keyup", "Control"));
    expect(setMuted).toHaveBeenLastCalledWith(true);
  });

  it("ignores keys other than Control", () => {
    const setMuted = vi.fn();
    renderHook(() => usePushToTalk(true, setMuted));
    setMuted.mockClear();

    act(() => fireKey("keydown", "Shift"));

    expect(setMuted).not.toHaveBeenCalled();
  });

  it("becoming inactive hands back an open mic (unmutes) instead of leaving it muted", () => {
    const setMuted = vi.fn();
    const { rerender } = renderHook(({ active }) => usePushToTalk(active, setMuted), {
      initialProps: { active: true },
    });
    setMuted.mockClear();

    rerender({ active: false });

    expect(setMuted).toHaveBeenCalledWith(false);
  });
});
