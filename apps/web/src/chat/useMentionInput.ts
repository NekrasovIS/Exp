// Issue #409 — extracted out of MessageComposer.tsx and
// EncryptedChatViewContent.tsx, which had independently reimplemented
// the exact same "wire useMentionAutocomplete() up to a plain <input>"
// logic. That duplication is what let the two copies drift: issue #369
// fixed a reproducible-on-CI input-scrambling bug in the plain composer
// by moving the post-pick caret repositioning from requestAnimationFrame
// (scheduled independently of React's own commit ordering) to a
// useLayoutEffect keyed on the body text (ordered relative to React's
// commit instead) — see the detailed history in this file's own
// pendingCursorPos comment below. EncryptedChatViewContent's copy never
// got that fix, since there was nothing forcing the two to stay in sync.
// Now there is only one copy, used by both composers.

import { useLayoutEffect, useRef, type KeyboardEvent } from "react";

import { useMentionAutocomplete } from "./useMentionAutocomplete.js";

/** Wires useMentionAutocomplete() up to a single-line <input> whose text
 * lives in the caller's own state (`body`/`setBody` — MessageComposer
 * persists it via useMessageDraft(), EncryptedChatViewContent just uses
 * useState(), so this hook doesn't own the value itself). Returns
 * everything needed to render the input and its suggestion popup:
 * `bodyInputRef` goes on the <input>, `handleKeyDown` on its onKeyDown,
 * `suggestions`/`activeIndex`/`selectMention` go straight to
 * <MentionSuggestions>. The caller is still responsible for calling
 * `notifyTextChanged` from its own onChange (after calling setBody) —
 * this hook doesn't own onChange either, since both callers also need
 * to run their own side effect there (onTyping()/sendTyping()). */
export function useMentionInput(communityId: number, body: string, setBody: (text: string) => void) {
  const mention = useMentionAutocomplete(communityId);
  const bodyInputRef = useRef<HTMLInputElement>(null);
  // Issue #369 — a mention-suggestion pick needs to move the caret to
  // right after the inserted "@login " (see selectMention()/
  // handleKeyDown() below), but that only works once the <input>'s DOM
  // value actually reflects the new `body` — setSelectionRange() on the
  // old value places the caret at a stale offset. Deferring that call
  // via requestAnimationFrame (the original approach, scheduled
  // independently of React's own commit ordering) hit a reproducible
  // scrambled-input failure on CI. A layout effect keyed on `body` is
  // ordered relative to React's own commit instead of the browser's
  // paint clock, which is the correct tool for "run after this state
  // update lands in the DOM" regardless of the exact CI mechanism.
  const pendingCursorPos = useRef<number | null>(null);

  useLayoutEffect(() => {
    if (pendingCursorPos.current === null) {
      return;
    }
    bodyInputRef.current?.setSelectionRange(pendingCursorPos.current, pendingCursorPos.current);
    pendingCursorPos.current = null;
  }, [body]);

  function notifyTextChanged(value: string, selectionStart: number): void {
    mention.handleTextChange(value, selectionStart);
  }

  function selectMention(login: string): void {
    const cursorPos = bodyInputRef.current?.selectionStart ?? body.length;
    const result = mention.applySuggestion(body, cursorPos, login);
    pendingCursorPos.current = result.cursorPos;
    setBody(result.text);
  }

  function handleKeyDown(event: KeyboardEvent<HTMLInputElement>): void {
    if (mention.suggestions.length === 0) {
      return;
    }
    if (event.key === "ArrowDown") {
      event.preventDefault();
      mention.moveActive(1);
    } else if (event.key === "ArrowUp") {
      event.preventDefault();
      mention.moveActive(-1);
    } else if (event.key === "Enter" || event.key === "Tab") {
      event.preventDefault();
      const cursorPos = bodyInputRef.current?.selectionStart ?? body.length;
      const result = mention.applyActive(body, cursorPos);
      pendingCursorPos.current = result.cursorPos;
      setBody(result.text);
    } else if (event.key === "Escape") {
      mention.dismiss();
    }
  }

  return {
    bodyInputRef,
    suggestions: mention.suggestions,
    activeIndex: mention.activeIndex,
    notifyTextChanged,
    selectMention,
    handleKeyDown,
  };
}
