// Issue #267 — renders the message history: edit is author-only,
// delete is author-or-moderator (see README's chat-service section),
// and a downloaded attachment (if any) as a plain link.
//
// Reactions (issue #305/#333/#335) are available on ANY message, own
// or not — unlike Edit/Delete, gating is not by author/moderator at
// all. The fixed 5-emoji set matches DeviceHub's own ChatMessageRow
// (reactionEmojis()) and the in-call reaction set (issue #312), so a
// user sees the same choices everywhere in the app.

import type { ChatMessageInfo } from "@devicehub/core";
import { useState } from "react";

import styles from "./MessageList.module.css";
import { AttachmentDownloadLink } from "./AttachmentDownloadLink.js";

const kReactionEmojis = ["👍", "❤️", "😂", "🎉", "👏"];

interface MessageListProps {
  messages: ChatMessageInfo[];
  editedIds: ReadonlySet<number>;
  currentLogin: string | null;
  isModerator: boolean;
  onEdit: (id: number, newBody: string) => void;
  onDelete: (id: number) => void;
  onToggleReaction: (id: number, emoji: string) => void;
}

export function MessageList({
  messages,
  editedIds,
  currentLogin,
  isModerator,
  onEdit,
  onDelete,
  onToggleReaction,
}: MessageListProps) {
  const [editingId, setEditingId] = useState<number | null>(null);
  const [draft, setDraft] = useState("");
  const [reactingId, setReactingId] = useState<number | null>(null);

  function pickReaction(id: number, emoji: string): void {
    onToggleReaction(id, emoji);
    setReactingId(null);
  }

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
                  <div className={styles.reactions}>
                    {message.reactions.map((reaction) => (
                      <button
                        key={reaction.emoji}
                        type="button"
                        className={styles.reactionChip}
                        title={reaction.logins.join(", ")}
                        onClick={() => onToggleReaction(message.id, reaction.emoji)}
                      >
                        {reaction.emoji} {reaction.logins.length}
                      </button>
                    ))}
                    {reactingId === message.id ? (
                      <span className={styles.reactPicker}>
                        {kReactionEmojis.map((emoji) => (
                          <button
                            key={emoji}
                            type="button"
                            className={styles.reactPickerEmoji}
                            onClick={() => pickReaction(message.id, emoji)}
                          >
                            {emoji}
                          </button>
                        ))}
                      </span>
                    ) : (
                      <button
                        type="button"
                        className={styles.actionButton}
                        onClick={() => setReactingId(message.id)}
                      >
                        React
                      </button>
                    )}
                  </div>
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
