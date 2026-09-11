// Issue #308/#338/#340 — list of the current channel's pinned
// messages, toggled from a "📌 N" button in the header (see
// ChatViewContent.tsx/EncryptedChatViewContent.tsx). Clicking an entry
// scrolls the corresponding row into view if it's currently loaded —
// MessageList gives each row `id="message-<id>"` for exactly this.

import type { PinnedMessageInfo } from "@devicehub/core";

import styles from "./PinnedMessagesPanel.module.css";

interface PinnedMessagesPanelProps {
  pinned: PinnedMessageInfo[];
}

export function PinnedMessagesPanel({ pinned }: PinnedMessagesPanelProps) {
  function jumpTo(id: number): void {
    document.getElementById(`message-${id}`)?.scrollIntoView({ block: "center" });
  }

  return (
    <div className={styles.panel}>
      <ul className={styles.list}>
        {pinned.length === 0 && <li className={styles.item}>No pinned messages.</li>}
        {pinned.map((message) => (
          <li key={message.id} className={styles.item}>
            <button type="button" className={styles.jumpButton} onClick={() => jumpTo(message.id)}>
              <strong>{message.author}</strong> {message.body}{" "}
              <span className={styles.pinnedBy}>📌 by {message.pinnedBy}</span>
            </button>
          </li>
        ))}
      </ul>
    </div>
  );
}
