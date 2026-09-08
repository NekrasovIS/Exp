// Issue #268 — DM threads' composer is text-only (no attachments,
// unlike channel chat's MessageComposer, #267).

import { useState, type FormEvent } from "react";

interface DirectMessageComposerProps {
  onSend: (body: string) => void;
}

export function DirectMessageComposer({ onSend }: DirectMessageComposerProps) {
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
    <form onSubmit={handleSubmit}>
      <label htmlFor="dm-body">Message</label>
      <input id="dm-body" value={body} onChange={(event) => setBody(event.target.value)} />
      <button type="submit">Send</button>
    </form>
  );
}
