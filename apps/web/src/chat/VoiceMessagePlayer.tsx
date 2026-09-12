// Issue #360 — renders a voice-message attachment as a playable
// <audio> element instead of AttachmentDownloadLink's "Download: ..."
// link. Bytes are fetched lazily on first "Play" click, same
// authenticated-Blob-URL pattern as AttachmentDownloadLink (and the
// same lazy-load-on-click precedent as desktop's playVoiceMessageButton
// — see ChatView.cpp's voicePlaybackRequested/setVoiceMessageData),
// rather than eagerly downloading every voice message a channel's
// history happens to render.

import { ChatRestClient } from "@devicehub/core";
import { useEffect, useMemo, useRef, useState } from "react";

import { chatServiceRestUrl } from "../config.js";
import { useSession } from "../session/SessionContext.js";

interface VoiceMessagePlayerProps {
  attachmentId: number;
  filename: string;
}

export function VoiceMessagePlayer({ attachmentId, filename }: VoiceMessagePlayerProps) {
  const { getAccessToken } = useSession();
  const client = useMemo(() => new ChatRestClient(chatServiceRestUrl), []);
  const [audioUrl, setAudioUrl] = useState<string | null>(null);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const audioUrlRef = useRef<string | null>(null);

  useEffect(() => {
    return () => {
      if (audioUrlRef.current !== null) {
        URL.revokeObjectURL(audioUrlRef.current);
      }
    };
  }, []);

  async function handlePlayClick(): Promise<void> {
    const token = getAccessToken();
    if (token === null) {
      return;
    }
    setError(null);
    setLoading(true);
    try {
      const bytes = await client.downloadAttachment(token, attachmentId);
      // Same ArrayBufferView<ArrayBuffer> vs. ArrayBufferView<ArrayBufferLike>
      // mismatch as AttachmentDownloadLink.tsx — see its own comment.
      const url = URL.createObjectURL(new Blob([bytes as BlobPart]));
      audioUrlRef.current = url;
      setAudioUrl(url);
    } catch {
      setError("Couldn't load that voice message.");
    } finally {
      setLoading(false);
    }
  }

  if (audioUrl !== null) {
    return <audio controls autoPlay src={audioUrl} aria-label={filename} />;
  }

  return (
    <>
      <button type="button" onClick={() => void handlePlayClick()} disabled={loading}>
        {loading ? "Loading…" : "▶ Play voice message"}
      </button>
      {error !== null && <span role="alert">{error}</span>}
    </>
  );
}
