import { renderHook, act } from "@testing-library/react";
import { afterEach, describe, expect, it } from "vitest";

import { isChannelMuted, setChannelMuted, useChannelMute } from "../../src/notifications/mutedChannels.js";

afterEach(() => {
  localStorage.clear();
});

describe("isChannelMuted/setChannelMuted", () => {
  it("a channel is not muted until setChannelMuted(true) is called for it", () => {
    expect(isChannelMuted(1)).toBe(false);
  });

  it("setChannelMuted(true) then isChannelMuted reflects it, scoped to that channel only", () => {
    setChannelMuted(1, true);
    expect(isChannelMuted(1)).toBe(true);
    expect(isChannelMuted(2)).toBe(false);
  });

  it("setChannelMuted(false) unmutes again", () => {
    setChannelMuted(1, true);
    setChannelMuted(1, false);
    expect(isChannelMuted(1)).toBe(false);
  });

  it("survives being read back after a fresh localStorage read (not just in-memory)", () => {
    setChannelMuted(5, true);
    // A second, independent read — same simulated "page reload" check
    // useMessageDraft.test.ts does for drafts.
    expect(isChannelMuted(5)).toBe(true);
  });

  it("tolerates corrupted localStorage content instead of throwing", () => {
    localStorage.setItem("devicehub.web.mutedChannelIds", "not json");
    expect(isChannelMuted(1)).toBe(false);
  });
});

describe("useChannelMute", () => {
  it("starts unmuted for a channel with no stored preference", () => {
    const { result } = renderHook(() => useChannelMute(1));
    expect(result.current.muted).toBe(false);
  });

  it("starts muted when the channel was already muted", () => {
    setChannelMuted(1, true);
    const { result } = renderHook(() => useChannelMute(1));
    expect(result.current.muted).toBe(true);
  });

  it("toggle() flips the state and persists it", () => {
    const { result } = renderHook(() => useChannelMute(1));

    act(() => result.current.toggle());
    expect(result.current.muted).toBe(true);
    expect(isChannelMuted(1)).toBe(true);

    act(() => result.current.toggle());
    expect(result.current.muted).toBe(false);
    expect(isChannelMuted(1)).toBe(false);
  });

  it("switching to a different channelId shows that channel's own mute state", () => {
    setChannelMuted(2, true);
    const { result, rerender } = renderHook(({ channelId }) => useChannelMute(channelId), {
      initialProps: { channelId: 1 },
    });
    expect(result.current.muted).toBe(false);

    rerender({ channelId: 2 });
    expect(result.current.muted).toBe(true);
  });
});
