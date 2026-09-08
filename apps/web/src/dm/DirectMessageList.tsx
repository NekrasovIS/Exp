// Issue #268 — renders one DM thread's history. No edit/delete/
// attachments here (DM threads don't support them — see
// useDirectMessages.ts's doc comment).

import type { DirectMessageInfo } from "@devicehub/core";

import styles from "../chat/MessageList.module.css";

interface DirectMessageListProps {
  messages: DirectMessageInfo[];
  currentLogin: string | null;
}

export function DirectMessageList({ messages, currentLogin }: DirectMessageListProps) {
  return (
    <ul className={styles.list}>
      {messages.map((message) => {
        const isOwn = message.author === currentLogin;
        return (
          <li key={message.id} className={`${styles.row} ${isOwn ? styles.rowOwn : ""}`}>
            <div className={`${styles.bubble} ${isOwn ? styles.bubbleOwn : ""}`}>
              <strong className={styles.author}>{message.author}</strong>
              <span>{message.body}</span>
            </div>
          </li>
        );
      })}
    </ul>
  );
}
