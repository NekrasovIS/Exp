import { useCallback, useEffect, useState } from "react";

// Issue #457 — per-channel notification mute. Today shouldNotify()
// (browserNotifications.ts) only ever fires for the channel currently
// open in this tab anyway (there's no live push for a channel without
// an active subscription — see chat-service docs' "Сознательно
// упрощено" note on unread counts) — so muting here specifically means
// "I have this channel open right now but don't want it pinging me
// while the tab is in the background", not "silence a channel I'm not
// looking at". Local-only (no backend/cross-device sync), same
// trade-off useMessageDraft.ts already makes for drafts.

const kStorageKey = "devicehub.web.mutedChannelIds";

function readMutedIds(): Set<number> {
  try {
    const raw = localStorage.getItem(kStorageKey);
    if (raw === null) {
      return new Set();
    }
    const parsed: unknown = JSON.parse(raw);
    return Array.isArray(parsed)
      ? new Set(parsed.filter((id): id is number => typeof id === "number"))
      : new Set();
  } catch {
    return new Set();
  }
}

function writeMutedIds(ids: ReadonlySet<number>): void {
  try {
    localStorage.setItem(kStorageKey, JSON.stringify(Array.from(ids)));
  } catch {
    // localStorage unavailable (private mode, quota) — the mute just
    // won't survive a reload, not worth surfacing as an error.
  }
}

/** Plain (non-reactive) check for use outside React render — the
 * `shouldNotify()` call site in useMessages.ts doesn't need to re-render
 * when the mute state changes, it only reads it at the moment a message
 * arrives. */
export function isChannelMuted(channelId: number): boolean {
  return readMutedIds().has(channelId);
}

export function setChannelMuted(channelId: number, muted: boolean): void {
  const ids = readMutedIds();
  if (muted) {
    ids.add(channelId);
  } else {
    ids.delete(channelId);
  }
  writeMutedIds(ids);
}

/** Reactive mute toggle for the channel header bell button — re-reads
 * on `channelId` change (switching channels shows that channel's own
 * mute state, not the previous one's). */
export function useChannelMute(channelId: number): { muted: boolean; toggle: () => void } {
  const [muted, setMuted] = useState(() => isChannelMuted(channelId));

  useEffect(() => {
    setMuted(isChannelMuted(channelId));
  }, [channelId]);

  const toggle = useCallback(() => {
    setMuted((previous) => {
      const next = !previous;
      setChannelMuted(channelId, next);
      return next;
    });
  }, [channelId]);

  return { muted, toggle };
}
