// Issue #268 — DM threads' composer is text-only (no attachments,
// unlike channel chat's MessageComposer, #267).
//
// The draft (issue #376) is persisted per thread via useMessageDraft(),
// the same mechanism MessageComposer.tsx uses for channel chat.

import { type FormEvent } from "react";

import styles from "../chat/chatView.module.css";
import { useMessageDraft } from "../chat/useMessageDraft.js";

interface DirectMessageComposerProps {
  threadId: number;
  onSend: (body: string) => void;
  /** Called on every keystroke (issue #313) — the hook (useDirectMessages)
   * is the one that throttles this down to a real WebSocket frame, this
   * component just reports every edit. Optional so existing callers/tests
   * that don't care about typing don't need to pass a no-op. */
  onTyping?: () => void;
}

export function DirectMessageComposer({ threadId, onSend, onTyping }: DirectMessageComposerProps) {
  const { draft: body, setDraft: setBody, clearDraft } = useMessageDraft(`dm:${threadId}`);

  function handleSubmit(event: FormEvent): void {
    event.preventDefault();
    if (body.trim() === "") {
      return;
    }
    onSend(body.trim());
    clearDraft();
  }

  return (
    <form onSubmit={handleSubmit} className={styles.simpleComposerForm}>
      <label htmlFor="dm-body">Message</label>
      <input
        id="dm-body"
        value={body}
        onChange={(event) => {
          setBody(event.target.value);
          onTyping?.();
        }}
      />
      <button type="submit">Send</button>
    </form>
  );
}
