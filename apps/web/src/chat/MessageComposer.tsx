// Issue #267 — the message input, mirroring DeviceHub's composer: a
// text field, an attach button (uploads immediately on file selection,
// then the file rides along as attachment_id on the next Send), and
// Send.

import { ChatRestClient } from "@devicehub/core";
import { useMemo, useState, type FormEvent } from "react";

import { chatServiceRestUrl } from "../config.js";
import { useSession } from "../session/SessionContext.js";

interface MessageComposerProps {
  channelId: number;
  onSend: (body: string, attachmentId?: number) => void;
}

export function MessageComposer({ channelId, onSend }: MessageComposerProps) {
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
  }

  return (
    <form onSubmit={handleSubmit}>
      {error !== null && <p role="alert">{error}</p>}
      {pendingAttachment !== null && (
        <p>
          Attached: {pendingAttachment.filename}{" "}
          <button type="button" onClick={() => setPendingAttachment(null)}>
            Remove
          </button>
        </p>
      )}
      <label htmlFor="message-body">Message</label>
      <input id="message-body" value={body} onChange={(event) => setBody(event.target.value)} />
      <label htmlFor="message-attachment">Attach a file</label>
      <input id="message-attachment" type="file" onChange={(event) => void handleFileSelected(event)} />
      <button type="submit" disabled={uploading}>
        Send
      </button>
    </form>
  );
}
