// Issue #268 — renders one DM thread's history. No edit/delete/
// attachments here (DM threads don't support them — see
// useDirectMessages.ts's doc comment).

import type { DirectMessageInfo } from "@devicehub/core";
import { MessageRow, messageRowStyles as rowStyles } from "@devicehub/ui";

interface DirectMessageListProps {
  messages: DirectMessageInfo[];
  currentLogin: string | null;
}

export function DirectMessageList({ messages, currentLogin }: DirectMessageListProps) {
  return (
    <ul className={rowStyles.list}>
      {messages.map((message) => {
        const isOwn = message.author === currentLogin;
        return (
          <MessageRow key={message.id} isOwn={isOwn} author={message.author}>
            <span>{message.body}</span>
          </MessageRow>
        );
      })}
    </ul>
  );
}
