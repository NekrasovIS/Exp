// Issue #267 — the message input, mirroring DeviceHub's composer: a
// text field, an attach button (uploads immediately on file selection,
// then the file rides along as attachment_id on the next Send), and
// Send.
//
// @mention autocomplete (issue #326) is layered onto the same <input>
// via useMentionAutocomplete() — see that hook's own doc comment for
// why it doesn't (yet) reuse #322's useMembers().

import { ChatRestClient } from "@devicehub/core";
import { useMemo, useRef, useState, type FormEvent, type KeyboardEvent } from "react";

import styles from "./MessageComposer.module.css";
import { MentionSuggestions } from "./MentionSuggestions.js";
import { useMentionAutocomplete } from "./useMentionAutocomplete.js";
import { chatServiceRestUrl } from "../config.js";
import { useSession } from "../session/SessionContext.js";

interface MessageComposerProps {
  channelId: number;
  communityId: number;
  onSend: (body: string, attachmentId?: number) => void;
}

export function MessageComposer({ channelId, communityId, onSend }: MessageComposerProps) {
  const { getAccessToken } = useSession();
  const client = useMemo(() => new ChatRestClient(chatServiceRestUrl), []);
  const mention = useMentionAutocomplete(communityId);
  const bodyInputRef = useRef<HTMLInputElement>(null);

  const [body, setBody] = useState("");
  const [pendingAttachment, setPendingAttachment] = useState<{ id: number; filename: string } | null>(null);
  const [uploading, setUploading] = useState(false);
  const [error, setError] = useState<string | null>(null);

  async function handleFileSelected(event: React.ChangeEvent<HTMLInputElement>): Promise<void> {
    const file = event.target.files?.[0];
    event.target.value = "";
    if (file === undefined) {
      return;
    }
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

  function handleSubmit(event: FormEvent): void {
    event.preventDefault();
    if (body.trim() === "" && pendingAttachment === null) {
      return;
    }
    onSend(body.trim(), pendingAttachment?.id);
    setBody("");
    setPendingAttachment(null);
  }

  function handleBodyChange(event: React.ChangeEvent<HTMLInputElement>): void {
    setBody(event.target.value);
    mention.handleTextChange(event.target.value, event.target.selectionStart ?? event.target.value.length);
  }

  function selectMention(login: string): void {
    const cursorPos = bodyInputRef.current?.selectionStart ?? body.length;
    const result = mention.applySuggestion(body, cursorPos, login);
    setBody(result.text);
    requestAnimationFrame(() => bodyInputRef.current?.setSelectionRange(result.cursorPos, result.cursorPos));
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
      setBody(result.text);
      requestAnimationFrame(() =>
        bodyInputRef.current?.setSelectionRange(result.cursorPos, result.cursorPos),
      );
    } else if (event.key === "Escape") {
      mention.dismiss();
    }
  }

  return (
    <>
      {error !== null && <p role="alert">{error}</p>}
      {pendingAttachment !== null && (
        <p className={styles.pendingAttachment}>
          Attached: {pendingAttachment.filename}{" "}
          <button type="button" onClick={() => setPendingAttachment(null)}>
            Remove
          </button>
        </p>
      )}
      <form onSubmit={handleSubmit} className={styles.form}>
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
