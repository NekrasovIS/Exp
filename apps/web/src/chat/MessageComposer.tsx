// Issue #267 — the message input, mirroring DeviceHub's composer: a
// text field, an attach button (uploads immediately on file selection,
// then the file rides along as attachment_id on the next Send), and
// Send.

import { ChatRestClient } from "@devicehub/core";
import { useMemo, useState, type FormEvent } from "react";

import styles from "./MessageComposer.module.css";
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
  onSend: (body: string, attachmentId?: number) => void;
  replyTarget?: ReplyTarget | null;
  onCancelReply?: () => void;
}

export function MessageComposer({ channelId, onSend, replyTarget, onCancelReply }: MessageComposerProps) {
  const { getAccessToken } = useSession();
  const client = useMemo(() => new ChatRestClient(chatServiceRestUrl), []);

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
    onCancelReply?.();
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
          className={styles.bodyInput}
          value={body}
          onChange={(event) => setBody(event.target.value)}
        />
        <button type="submit" disabled={uploading}>
          Send
        </button>
      </form>
    </>
  );
}
