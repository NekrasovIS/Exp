// Issue #267 — renders the message history: edit is author-only,
// delete is author-or-moderator (see README's chat-service section),
// and a downloaded attachment (if any) as a plain link.

import type { ChatMessageInfo } from "@devicehub/core";
import { useState } from "react";

import { AttachmentDownloadLink } from "./AttachmentDownloadLink.js";

interface MessageListProps {
  messages: ChatMessageInfo[];
  editedIds: ReadonlySet<number>;
  currentLogin: string | null;
  isModerator: boolean;
  onEdit: (id: number, newBody: string) => void;
  onDelete: (id: number) => void;
}

export function MessageList({
  messages,
  editedIds,
  currentLogin,
  isModerator,
  onEdit,
  onDelete,
}: MessageListProps) {
  const [editingId, setEditingId] = useState<number | null>(null);
  const [draft, setDraft] = useState("");

  function startEditing(message: ChatMessageInfo): void {
    setEditingId(message.id);
    setDraft(message.body);
  }

  function commitEdit(id: number): void {
    if (draft.trim() !== "") {
      onEdit(id, draft.trim());
    }
    setEditingId(null);
  }

  return (
    <ul>
      {messages.map((message) => {
        const isOwn = message.author === currentLogin;
        return (
          <li key={message.id}>
            <strong>{message.author}</strong>{" "}
            {editingId === message.id ? (
              <>
                <input value={draft} onChange={(event) => setDraft(event.target.value)} />
                <button type="button" onClick={() => commitEdit(message.id)}>
                  Save
                </button>
                <button type="button" onClick={() => setEditingId(null)}>
                  Cancel
                </button>
              </>
            ) : (
              <>
                <span>{message.body}</span>
                {editedIds.has(message.id) && <em> (edited)</em>}
                {message.attachmentId !== undefined && message.attachmentFilename !== undefined && (
                  <AttachmentDownloadLink
                    attachmentId={message.attachmentId}
                    filename={message.attachmentFilename}
                  />
                )}
                {isOwn && (
                  <button type="button" onClick={() => startEditing(message)}>
                    Edit
                  </button>
                )}
                {(isOwn || isModerator) && (
                  <button type="button" onClick={() => onDelete(message.id)}>
                    Delete
                  </button>
                )}
              </>
            )}
          </li>
        );
      })}
    </ul>
  );
}
