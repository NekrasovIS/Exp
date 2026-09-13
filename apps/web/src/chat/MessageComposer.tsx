// Issue #267 — the message input, mirroring DeviceHub's composer: a
// text field, an attach button (uploads immediately on file selection,
// then the file rides along as attachment_id on the next Send), and
// Send.
//
// @mention autocomplete (issue #326) is layered onto the same <input>
// via useMentionAutocomplete() — see that hook's own doc comment for
// why it doesn't (yet) reuse #322's useMembers().
//
// Drag-and-drop (issue #377) is an alternate entry point onto the same
// upload path as the "Attach a file" input — see uploadFile() below,
// shared by both.
//
// The draft (issue #376) is the unsent `body` text, persisted per
// channel via useMessageDraft() so it survives switching to another
// channel and back (or a page reload) before Send.

import { ChatRestClient } from "@devicehub/core";
import { useMemo, useState, type DragEvent, type FormEvent } from "react";

import styles from "./MessageComposer.module.css";
import { MentionSuggestions } from "./MentionSuggestions.js";
import { useMentionInput } from "./useMentionInput.js";
import { useMessageDraft } from "./useMessageDraft.js";
import { chatServiceRestUrl } from "../config.js";
import { useSession } from "../session/SessionContext.js";

/** Bare minimum ChatViewContent needs to resolve/display the current
 * reply target (issue #306/#331) — mirrors DeviceHub's ChatView, which
 * resolves the same author/snippet from its own already-loaded
 * history before ever handing it to the composer. */
interface ReplyTarget {
  id: number;
  author: string;
  snippet: string;
}

interface MessageComposerProps {
  channelId: number;
  communityId: number;
  onSend: (body: string, attachmentId?: number) => void;
  replyTarget?: ReplyTarget | null;
  onCancelReply?: () => void;
  /** Called on every keystroke (issue #318) — the hook (useMessages) is
   * the one that throttles this down to a real WebSocket frame, this
   * component just reports every edit. Optional so existing callers/tests
   * that don't care about typing don't need to pass a no-op. */
  onTyping?: () => void;
}

export function MessageComposer({
  channelId,
  communityId,
  onSend,
  replyTarget,
  onCancelReply,
  onTyping,
}: MessageComposerProps) {
  const { getAccessToken } = useSession();
  const client = useMemo(() => new ChatRestClient(chatServiceRestUrl), []);
  const { draft: body, setDraft: setBody, clearDraft } = useMessageDraft(`channel:${channelId}`);
  const mentionInput = useMentionInput(communityId, body, setBody);
  const [pendingAttachment, setPendingAttachment] = useState<{ id: number; filename: string } | null>(null);
  const [uploading, setUploading] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [draggingOver, setDraggingOver] = useState(false);

  async function uploadFile(file: File): Promise<void> {
    const token = getAccessToken();
    if (token === null) {
      return;
    }
    setError(null);
    setUploading(true);
    try {
      const bytes = new Uint8Array(await file.arrayBuffer());
      const uploaded = await client.uploadAttachment(token, channelId, file.name, file.type, bytes);
      setPendingAttachment(uploaded);
    } catch {
      setError("Couldn't upload that file.");
    } finally {
      setUploading(false);
    }
  }

  async function handleFileSelected(event: React.ChangeEvent<HTMLInputElement>): Promise<void> {
    const file = event.target.files?.[0];
    event.target.value = "";
    if (file === undefined) {
      return;
    }
    await uploadFile(file);
  }

  function handleDragOver(event: DragEvent<HTMLFormElement>): void {
    // Required for the browser to treat this element as a valid drop
    // target at all — without preventDefault() here, handleDrop()'s own
    // preventDefault() never gets a chance to run; the browser instead
    // navigates to/opens the dropped file.
    event.preventDefault();
    setDraggingOver(true);
  }

  function handleDragLeave(event: DragEvent<HTMLFormElement>): void {
    // dragenter/dragleave fire per-element and bubble, so moving from
    // the form onto one of its own children (the file input's label,
    // the message input, …) fires a dragleave here too — only actually
    // clear the highlight once the pointer leaves the form itself.
    if (event.currentTarget.contains(event.relatedTarget as Node | null)) {
      return;
    }
    setDraggingOver(false);
  }

  async function handleDrop(event: DragEvent<HTMLFormElement>): Promise<void> {
    event.preventDefault();
    setDraggingOver(false);
    const file = event.dataTransfer.files[0];
    if (file === undefined) {
      return;
    }
    await uploadFile(file);
  }

  function handleSubmit(event: FormEvent): void {
    event.preventDefault();
    if (body.trim() === "" && pendingAttachment === null) {
      return;
    }
    onSend(body.trim(), pendingAttachment?.id);
    clearDraft();
    setPendingAttachment(null);
    onCancelReply?.();
  }

  function handleBodyChange(event: React.ChangeEvent<HTMLInputElement>): void {
    setBody(event.target.value);
    onTyping?.();
    mentionInput.notifyTextChanged(event.target.value, event.target.selectionStart ?? event.target.value.length);
  }

  return (
    <>
      {error !== null && <p role="alert">{error}</p>}
      {replyTarget != null && (
        <p className={styles.replyBar}>
          Replying to <strong>{replyTarget.author}</strong>: {replyTarget.snippet}{" "}
          <button type="button" onClick={onCancelReply}>
            Cancel
          </button>
        </p>
      )}
      {pendingAttachment !== null && (
        <p className={styles.pendingAttachment}>
          Attached: {pendingAttachment.filename}{" "}
          <button type="button" onClick={() => setPendingAttachment(null)}>
            Remove
          </button>
        </p>
      )}
      <form
        onSubmit={handleSubmit}
        onDragOver={handleDragOver}
        onDragLeave={handleDragLeave}
        onDrop={(event) => void handleDrop(event)}
        data-dragover={draggingOver}
        className={styles.form}
      >
        {draggingOver && (
          <p className={styles.dropHint} aria-hidden="true">
            Drop to attach
          </p>
        )}
        <label htmlFor="message-attachment" className={styles.attachLabel}>
          Attach a file
        </label>
        <input
          id="message-attachment"
          className={styles.attachInput}
          type="file"
          onChange={(event) => void handleFileSelected(event)}
        />
        <label htmlFor="message-body" className={styles.bodyLabel}>
          Message
        </label>
        <input
          id="message-body"
          ref={mentionInput.bodyInputRef}
          className={styles.bodyInput}
          value={body}
          onChange={handleBodyChange}
          onKeyDown={mentionInput.handleKeyDown}
        />
        <MentionSuggestions
          suggestions={mentionInput.suggestions}
          activeIndex={mentionInput.activeIndex}
          onSelect={mentionInput.selectMention}
        />
        <button type="submit" disabled={uploading}>
          Send
        </button>
      </form>
    </>
  );
}
