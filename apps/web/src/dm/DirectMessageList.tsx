// Issue #268 — renders one DM thread's history. No edit/delete/
// attachments here (DM threads don't support them — see
// useDirectMessages.ts's doc comment).

import type { DirectMessageInfo } from "@devicehub/core";

interface DirectMessageListProps {
  messages: DirectMessageInfo[];
}

export function DirectMessageList({ messages }: DirectMessageListProps) {
  return (
    <ul>
      {messages.map((message) => (
        <li key={message.id}>
          <strong>{message.author}</strong> <span>{message.body}</span>
        </li>
      ))}
    </ul>
  );
}
