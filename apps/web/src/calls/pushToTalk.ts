// Issue #461 — push-to-talk: hold Control to transmit, release to go
// back to muted. Control (not a printable key) is safe to hold down
// even while a text <input> has focus (typing a chat message during a
// call is a normal thing to do) — a printable key would insert itself
// into whatever's focused instead.

import { useEffect, useState } from "react";

const kPreferenceStorageKey = "devicehub.web.pushToTalkEnabled";
const kPushToTalkKey = "Control";

function readPreference(): boolean {
  try {
    return localStorage.getItem(kPreferenceStorageKey) === "true";
  } catch {
    return false;
  }
}

/** The on/off preference itself — a call-independent setting (kept
 * across joining/leaving calls), same local-only trade-off as issue
 * #457's per-channel mute. */
export function usePushToTalkPreference(): { enabled: boolean; setEnabled: (enabled: boolean) => void } {
  const [enabled, setEnabledState] = useState(readPreference);

  function setEnabled(next: boolean): void {
    setEnabledState(next);
    try {
      localStorage.setItem(kPreferenceStorageKey, String(next));
    } catch {
      // localStorage unavailable (private mode, quota) — the
      // preference just won't survive a reload.
    }
  }

  return { enabled, setEnabled };
}

/** Wires the actual hold-to-talk behavior up to @p setMuted (CallActions
 * from useCall.ts) while @p active — true only once BOTH the preference
 * is on AND a call is actually joined, so a stray keydown before/after
 * a call has nothing to do. Entering active mutes immediately (the
 * steady "not held" state); leaving it (preference turned off, or the
 * call ended) unmutes — so turning push-to-talk off hands back a normal
 * open mic instead of leaving it silently muted forever. */
export function usePushToTalk(active: boolean, setMuted: (muted: boolean) => void): void {
  useEffect(() => {
    if (!active) {
      return;
    }
    setMuted(true);

    function handleKeyDown(event: KeyboardEvent): void {
      if (event.key === kPushToTalkKey) {
        setMuted(false);
      }
    }
    function handleKeyUp(event: KeyboardEvent): void {
      if (event.key === kPushToTalkKey) {
        setMuted(true);
      }
    }
    document.addEventListener("keydown", handleKeyDown);
    document.addEventListener("keyup", handleKeyUp);
    return () => {
      document.removeEventListener("keydown", handleKeyDown);
      document.removeEventListener("keyup", handleKeyUp);
      setMuted(false);
    };
  }, [active, setMuted]);
}
