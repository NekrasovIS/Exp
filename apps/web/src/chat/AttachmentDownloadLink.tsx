// Issue #267 — downloadAttachment() returns raw bytes (not a URL,
// chat-service requires a bearer token to fetch them — see its own
// doc comment), so this fetches on click and hands the browser a
// Blob URL rather than linking directly to /attachments/{id}.

import { ChatRestClient } from "@devicehub/core";
import { useMemo, useState } from "react";

import { chatServiceRestUrl } from "../config.js";
import { useSession } from "../session/SessionContext.js";

interface AttachmentDownloadLinkProps {
  attachmentId: number;
  filename: string;
}

export function AttachmentDownloadLink({ attachmentId, filename }: AttachmentDownloadLinkProps) {
  const { getAccessToken } = useSession();
  const client = useMemo(() => new ChatRestClient(chatServiceRestUrl), []);
  const [error, setError] = useState<string | null>(null);

  async function handleClick(): Promise<void> {
    const token = getAccessToken();
    if (token === null) {
      return;
    }
    setError(null);
    try {
      const bytes = await client.downloadAttachment(token, attachmentId);
      // TS's DOM lib types BlobPart as ArrayBufferView<ArrayBuffer> —
      // Uint8Array is generically ArrayBufferView<ArrayBufferLike>,
      // which also admits SharedArrayBuffer, so the two don't unify
      // even though this one is always backed by a real ArrayBuffer
      // (it's freshly constructed from Response.arrayBuffer() inside
      // downloadAttachment(), never a view into shared memory).
      const url = URL.createObjectURL(new Blob([bytes as BlobPart]));
      const link = document.createElement("a");
      link.href = url;
      link.download = filename;
      link.click();
      URL.revokeObjectURL(url);
    } catch {
      setError("Couldn't download that file.");
    }
  }

  return (
    <>
      <button type="button" onClick={() => void handleClick()}>
        Download: {filename}
      </button>
      {error !== null && <span role="alert">{error}</span>}
    </>
  );
}
