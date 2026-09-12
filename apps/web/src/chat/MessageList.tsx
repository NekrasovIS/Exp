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
//
// Reactions (issue #305/#333/#335) are available on ANY message, own
// or not — unlike Edit/Delete, gating is not by author/moderator at
// all. The fixed 5-emoji set matches DeviceHub's own ChatMessageRow
// (reactionEmojis()) and the in-call reaction set (issue #312), so a
// user sees the same choices everywhere in the app.
//
// Pin/Unpin (issue #308/#338/#340) is role-gated the same way delete
// is — reuses the same `isModerator` flag, not a separate check —
// but unlike delete, it's independent of authorship: pinning is a
// channel-management action, not message moderation, so it never
// shows just because the viewer happens to be the author.
//
// Voice messages (issue #360) render as VoiceMessagePlayer instead of
// AttachmentDownloadLink when isVoiceMessageAttachment() recognizes the
// filename — same branch point desktop's ChatMessageRow uses for its
// own isAudioAttachment() check.

import type { ChatMessageInfo } from "@devicehub/core";
import { useMemo, useState } from "react";

import styles from "./MessageList.module.css";
import { AttachmentDownloadLink } from "./AttachmentDownloadLink.js";
import { isVoiceMessageAttachment } from "./isVoiceMessageAttachment.js";
import { MessageBody } from "./MessageBody.js";
import { VoiceMessagePlayer } from "./VoiceMessagePlayer.js";

const kReactionEmojis = ["👍", "❤️", "😂", "🎉", "👏"];

const kReplySnippetMaxChars = 60;

export function truncatedSnippet(body: string): string {
  return body.length > kReplySnippetMaxChars ? `${body.slice(0, kReplySnippetMaxChars)}…` : body;
}

interface MessageListProps {
  messages: ChatMessageInfo[];
  editedIds: ReadonlySet<number>;
  pinnedIds: ReadonlySet<number>;
  currentLogin: string | null;
  isModerator: boolean;
  onEdit: (id: number, newBody: string) => void;
  onDelete: (id: number) => void;
  onReply: (id: number) => void;
  onToggleReaction: (id: number, emoji: string) => void;
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
  onReply,
  onToggleReaction,
  onPin,
  onUnpin,
}: MessageListProps) {
  const [editingId, setEditingId] = useState<number | null>(null);
  const [draft, setDraft] = useState("");
  const [reactingId, setReactingId] = useState<number | null>(null);
  const messagesById = useMemo(() => new Map(messages.map((m) => [m.id, m])), [messages]);

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
                  <span>
                    <MessageBody text={message.body} />
                  </span>
                  {editedIds.has(message.id) && <em className={styles.edited}>(edited)</em>}
                  {message.attachmentId !== undefined && message.attachmentFilename !== undefined && (
                    <span className={styles.attachment}>
                      {isVoiceMessageAttachment(message.attachmentFilename) ? (
                        <VoiceMessagePlayer
                          attachmentId={message.attachmentId}
                          filename={message.attachmentFilename}
                        />
                      ) : (
                        <AttachmentDownloadLink
                          attachmentId={message.attachmentId}
                          filename={message.attachmentFilename}
                        />
                      )}
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
                    {isModerator && (
                      <button
                        type="button"
                        className={styles.actionButton}
                        onClick={() => (isPinned ? onUnpin(message.id) : onPin(message.id))}
                      >
                        {isPinned ? "Unpin" : "Pin"}
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
