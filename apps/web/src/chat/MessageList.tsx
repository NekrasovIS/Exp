// Issue #267 — renders the message history: edit is author-only,
// delete is author-or-moderator (see README's chat-service section),
// and a downloaded attachment (if any) as a plain link.
//
// Reply/quote (issue #306/#331) is available on ANY message, own or
// not, same as Edit/Delete are not. The quote above a replying
// message's body is resolved from `messages` itself — chat-service
// never stores/sends a snapshot of the original author/body, only the
// bare id, so a reply to something outside the currently loaded page
// of history shows "Message unavailable" rather than fetching it
// specially (same client-side resolution model as DeviceHub's
// ChatView::messagesById_).

import type { ChatMessageInfo } from "@devicehub/core";
import { useMemo, useState } from "react";

import styles from "./MessageList.module.css";
import { AttachmentDownloadLink } from "./AttachmentDownloadLink.js";

const kReplySnippetMaxChars = 60;

export function truncatedSnippet(body: string): string {
  return body.length > kReplySnippetMaxChars ? `${body.slice(0, kReplySnippetMaxChars)}…` : body;
}

interface MessageListProps {
  messages: ChatMessageInfo[];
  editedIds: ReadonlySet<number>;
  currentLogin: string | null;
  isModerator: boolean;
  onEdit: (id: number, newBody: string) => void;
  onDelete: (id: number) => void;
  onReply: (id: number) => void;
}

export function MessageList({
  messages,
  editedIds,
  currentLogin,
  isModerator,
  onEdit,
  onDelete,
  onReply,
}: MessageListProps) {
  const [editingId, setEditingId] = useState<number | null>(null);
  const [draft, setDraft] = useState("");
  const messagesById = useMemo(() => new Map(messages.map((m) => [m.id, m])), [messages]);

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
        return (
          <li key={message.id} className={`${styles.row} ${isOwn ? styles.rowOwn : ""}`}>
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
                  {message.replyToMessageId !== undefined &&
                    (() => {
                      const original = messagesById.get(message.replyToMessageId);
                      return (
                        <span className={styles.quote}>
                          {original !== undefined ? (
                            <>
                              <strong>{original.author}</strong>: {truncatedSnippet(original.body)}
                            </>
                          ) : (
                            <em>Message unavailable</em>
                          )}
                        </span>
                      );
                    })()}
                  <span>{message.body}</span>
                  {editedIds.has(message.id) && <em className={styles.edited}>(edited)</em>}
                  {message.attachmentId !== undefined && message.attachmentFilename !== undefined && (
                    <span className={styles.attachment}>
                      <AttachmentDownloadLink
                        attachmentId={message.attachmentId}
                        filename={message.attachmentFilename}
                      />
                    </span>
                  )}
                  <div className={styles.actions}>
                    <button type="button" className={styles.actionButton} onClick={() => onReply(message.id)}>
                      Reply
                    </button>
                    {isOwn && (
                      <button
                        type="button"
                        className={styles.actionButton}
                        onClick={() => startEditing(message)}
                      >
                        Edit
                      </button>
                    )}
                    {(isOwn || isModerator) && (
                      <button
                        type="button"
                        className={styles.actionButton}
                        onClick={() => onDelete(message.id)}
                      >
                        Delete
                      </button>
                    )}
                  </div>
                </>
              )}
            </div>
          </li>
        );
      })}
    </ul>
  );
}
