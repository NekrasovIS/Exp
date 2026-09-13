// Issue #376 — persists a composer's unsent text across channel/thread
// switches (and page reloads): today it's just React state, gone the
// moment the user looks at another channel before hitting Send.
//
// Debounced (not written on every keystroke, per the issue's own
// scoping) and namespaced by the caller's own key so MessageComposer.tsx
// (channel chat, key `channel:<id>`) and DirectMessageComposer.tsx (DM
// thread, key `dm:<id>`) can share this without colliding.
import { useEffect, useState } from "react";

const kStorageKeyPrefix = "devicehub.web.draft.";
const kDebounceMs = 500;

function storageKey(key: string): string {
  return kStorageKeyPrefix + key;
}

function readDraft(key: string): string {
  try {
    return localStorage.getItem(storageKey(key)) ?? "";
  } catch {
    return "";
  }
}

let pendingWrite: { key: string; timeout: ReturnType<typeof setTimeout> } | null = null;

function scheduleWrite(key: string, value: string): void {
  if (pendingWrite !== null) {
    clearTimeout(pendingWrite.timeout);
  }
  const timeout = setTimeout(() => {
    pendingWrite = null;
    try {
      if (value === "") {
        localStorage.removeItem(storageKey(key));
      } else {
        localStorage.setItem(storageKey(key), value);
      }
    } catch {
      // localStorage unavailable (private mode, quota) — the draft
      // just won't survive a reload, not worth surfacing as an error.
    }
  }, kDebounceMs);
  pendingWrite = { key, timeout };
}

export interface MessageDraft {
  draft: string;
  setDraft: (value: string) => void;
  clearDraft: () => void;
}

/** One composer instance's draft, keyed by @p key (e.g. `channel:7`).
 * Switching `key` while mounted (same composer, different channel/thread)
 * reloads that key's own stored draft instead of carrying over the
 * previous one. */
export function useMessageDraft(key: string): MessageDraft {
  const [draft, setDraftState] = useState(() => readDraft(key));

  useEffect(() => {
    setDraftState(readDraft(key));
  }, [key]);

  function setDraft(value: string): void {
    setDraftState(value);
    scheduleWrite(key, value);
  }

  function clearDraft(): void {
    setDraftState("");
    if (pendingWrite?.key === key) {
      clearTimeout(pendingWrite.timeout);
      pendingWrite = null;
    }
    try {
      localStorage.removeItem(storageKey(key));
    } catch {
      // see scheduleWrite()'s catch above.
    }
  }

  return { draft, setDraft, clearDraft };
}
