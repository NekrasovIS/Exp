// Issue #267 — renders the message history: edit is author-only,
// delete is author-or-moderator (see README's chat-service section),
// and a downloaded attachment (if any) as a plain link.
//
// Pin/Unpin (issue #308/#338/#340) is role-gated the same way delete
// is — reuses the same `isModerator` flag, not a separate check —
// but unlike delete, it's independent of authorship: pinning is a
// channel-management action, not message moderation, so it never
// shows just because the viewer happens to be the author.

import type { ChatMessageInfo } from "@devicehub/core";
import { useState } from "react";

import styles from "./MessageList.module.css";
import { AttachmentDownloadLink } from "./AttachmentDownloadLink.js";
import { MessageBody } from "./MessageBody.js";

interface MessageListProps {
  messages: ChatMessageInfo[];
  editedIds: ReadonlySet<number>;
  pinnedIds: ReadonlySet<number>;
  currentLogin: string | null;
  isModerator: boolean;
  onEdit: (id: number, newBody: string) => void;
  onDelete: (id: number) => void;
  onPin: (id: number) => void;
  onUnpin: (id: number) => void;
}

export function MessageList({
  messages,
  editedIds,
  pinnedIds,
  currentLogin,
  isModerator,
  onEdit,
  onDelete,
  onPin,
  onUnpin,
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
    <ul className={styles.list}>
      {messages.map((message) => {
        const isOwn = message.author === currentLogin;
        const isPinned = pinnedIds.has(message.id);
        return (
          <li
            key={message.id}
            id={`message-${message.id}`}
            className={`${styles.row} ${isOwn ? styles.rowOwn : ""}`}
          >
            <div className={`${styles.bubble} ${isOwn ? styles.bubbleOwn : ""}`}>
              <strong className={styles.author}>{message.author}</strong>
              {editingId === message.id ? (
                <>
                  <input
                    className={styles.editInput}
                    value={draft}
                    onChange={(event) => setDraft(event.target.value)}
                  />
                  <div className={styles.actions}>
                    <button
                      type="button"
                      className={styles.actionButton}
                      onClick={() => commitEdit(message.id)}
                    >
                      Save
                    </button>
                    <button type="button" className={styles.actionButton} onClick={() => setEditingId(null)}>
                      Cancel
                    </button>
                  </div>
                </>
              ) : (
                <>
                  {isPinned && <span className={styles.pinned}>📌 Pinned</span>}
                  <span>
                    <MessageBody text={message.body} />
                  </span>
                  {editedIds.has(message.id) && <em className={styles.edited}>(edited)</em>}
                  {message.attachmentId !== undefined && message.attachmentFilename !== undefined && (
                    <span className={styles.attachment}>
                      <AttachmentDownloadLink
                        attachmentId={message.attachmentId}
                        filename={message.attachmentFilename}
                      />
                    </span>
                  )}
                  {(isOwn || isModerator) && (
                    <div className={styles.actions}>
                      {isOwn && (
                        <button
                          type="button"
                          className={styles.actionButton}
                          onClick={() => startEditing(message)}
                        >
                          Edit
                        </button>
                      )}
                      {isModerator && (
                        <button
                          type="button"
                          className={styles.actionButton}
                          onClick={() => (isPinned ? onUnpin(message.id) : onPin(message.id))}
                        >
                          {isPinned ? "Unpin" : "Pin"}
                        </button>
                      )}
                      <button
                        type="button"
                        className={styles.actionButton}
                        onClick={() => onDelete(message.id)}
                      >
                        Delete
                      </button>
                    </div>
                  )}
                </>
              )}
            </div>
          </li>
        );
      })}
    </ul>
  );
}
