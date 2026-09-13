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
import {
  useLayoutEffect,
  useMemo,
  useRef,
  useState,
  type DragEvent,
  type FormEvent,
  type KeyboardEvent,
} from "react";

import styles from "./MessageComposer.module.css";
import { MentionSuggestions } from "./MentionSuggestions.js";
import { useMentionAutocomplete } from "./useMentionAutocomplete.js";
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
  const mention = useMentionAutocomplete(communityId);
  const bodyInputRef = useRef<HTMLInputElement>(null);
  // Issue #369 — a mention-suggestion pick needs to move the caret to
  // right after the inserted "@login " (see selectMention()/
  // handleBodyKeyDown() below), but that only works once the <input>'s
  // DOM value actually reflects the new `body` — setSelectionRange() on
  // the old value places the caret at a stale offset. The previous
  // approach deferred that call via requestAnimationFrame, scheduled
  // independently of React's own commit ordering; CI's test suite hit a
  // scrambled-input failure here reproducibly (never locally, and not
  // reproduced under a stubbed/delayed requestAnimationFrame either, so
  // the exact mechanism on CI's runner isn't fully confirmed) that a RAF
  // ordered independently of React is at least consistent with. A
  // layout effect keyed on `body` is ordered relative to React's own
  // commit instead of the browser's paint clock, which is the more
  // correct tool for "run after this state update lands in the DOM"
  // regardless — it removes a real source of scheduling uncertainty
  // even though this specific CI failure's root cause wasn't nailed
  // down with full certainty.
  const pendingCursorPos = useRef<number | null>(null);

  const { draft: body, setDraft: setBody, clearDraft } = useMessageDraft(`channel:${channelId}`);
  const [pendingAttachment, setPendingAttachment] = useState<{ id: number; filename: string } | null>(null);
  const [uploading, setUploading] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [draggingOver, setDraggingOver] = useState(false);

  useLayoutEffect(() => {
    if (pendingCursorPos.current === null) {
      return;
    }
    bodyInputRef.current?.setSelectionRange(pendingCursorPos.current, pendingCursorPos.current);
    pendingCursorPos.current = null;
  }, [body]);

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
    mention.handleTextChange(event.target.value, event.target.selectionStart ?? event.target.value.length);
  }

  function selectMention(login: string): void {
    const cursorPos = bodyInputRef.current?.selectionStart ?? body.length;
    const result = mention.applySuggestion(body, cursorPos, login);
    pendingCursorPos.current = result.cursorPos;
    setBody(result.text);
  }

  function handleBodyKeyDown(event: KeyboardEvent<HTMLInputElement>): void {
    if (mention.suggestions.length === 0) {
      return;
    }
    if (event.key === "ArrowDown") {
      event.preventDefault();
      mention.moveActive(1);
    } else if (event.key === "ArrowUp") {
      event.preventDefault();
      mention.moveActive(-1);
    } else if (event.key === "Enter" || event.key === "Tab") {
      event.preventDefault();
      const cursorPos = bodyInputRef.current?.selectionStart ?? body.length;
      const result = mention.applyActive(body, cursorPos);
      pendingCursorPos.current = result.cursorPos;
      setBody(result.text);
    } else if (event.key === "Escape") {
      mention.dismiss();
    }
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
          ref={bodyInputRef}
          className={styles.bodyInput}
          value={body}
          onChange={handleBodyChange}
          onKeyDown={handleBodyKeyDown}
        />
        <MentionSuggestions
          suggestions={mention.suggestions}
          activeIndex={mention.activeIndex}
          onSelect={selectMention}
        />
        <button type="submit" disabled={uploading}>
          Send
        </button>
      </form>
    </>
  );
}
