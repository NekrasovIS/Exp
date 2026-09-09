// Issue #268 — DM threads' composer is text-only (no attachments,
// unlike channel chat's MessageComposer, #267).

import { useState, type FormEvent } from "react";

import styles from "../chat/chatView.module.css";

interface DirectMessageComposerProps {
  onSend: (body: string) => void;
  /** Called on every keystroke (issue #313) — the hook (useDirectMessages)
   * is the one that throttles this down to a real WebSocket frame, this
   * component just reports every edit. Optional so existing callers/tests
   * that don't care about typing don't need to pass a no-op. */
  onTyping?: () => void;
}

export function DirectMessageComposer({ onSend, onTyping }: DirectMessageComposerProps) {
  const [body, setBody] = useState("");

  function handleSubmit(event: FormEvent): void {
    event.preventDefault();
    if (body.trim() === "") {
      return;
    }
    onSend(body.trim());
    setBody("");
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
