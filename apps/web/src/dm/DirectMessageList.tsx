// Issue #268 — renders one DM thread's history. No edit/delete/
// attachments here (DM threads don't support them — see
// useDirectMessages.ts's doc comment).

import type { DirectMessageInfo } from "@devicehub/core";
import { MessageRow, messageRowStyles as rowStyles } from "@devicehub/ui";

import styles from "./DirectMessageList.module.css";

interface DirectMessageListProps {
  messages: DirectMessageInfo[];
  currentLogin: string | null;
  otherLogin: string;
  // issue #380 — a DM thread has exactly one other participant, so
  // there's nothing to enumerate the way MessageList.tsx does for a
  // channel: "seen" for one's own message is just whether otherLogin's
  // pointer has reached it.
  readPointers: ReadonlyMap<string, number>;
}

export function DirectMessageList({ messages, currentLogin, otherLogin, readPointers }: DirectMessageListProps) {
  const otherLastRead = readPointers.get(otherLogin) ?? -1;
  return (
    <ul className={rowStyles.list}>
      {messages.map((message) => {
        const isOwn = message.author === currentLogin;
        return (
          <MessageRow key={message.id} isOwn={isOwn} author={message.author}>
            <span>{message.body}</span>
            {isOwn && otherLastRead >= message.id && <span className={styles.seenBy}>Seen</span>}
          </MessageRow>
        );
      })}
    </ul>
  );
}
