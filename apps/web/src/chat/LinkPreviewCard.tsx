// Issue #396/#397 — a small card under a message's own text, shown
// once chat-service's GET /link-preview resolves a URL found in
// @p text. Renders nothing while pending and nothing at all if the
// server reports no preview (available: false) or the request itself
// fails — the message's plain text has already rendered either way, a
// missing card is never the only content for a link-bearing message.

import { ChatRestClient } from "@devicehub/core";
import type { LinkPreview } from "@devicehub/core";
import { useEffect, useMemo, useState } from "react";

import { findFirstUrl } from "./findFirstUrl.js";
import styles from "./LinkPreviewCard.module.css";
import { chatServiceRestUrl } from "../config.js";
import { useSession } from "../session/SessionContext.js";

interface LinkPreviewCardProps {
  text: string;
}

function extractDomain(url: string): string {
  try {
    return new URL(url).hostname;
  } catch {
    return url;
  }
}

export function LinkPreviewCard({ text }: LinkPreviewCardProps) {
  const { getAccessToken } = useSession();
  const client = useMemo(() => new ChatRestClient(chatServiceRestUrl), []);
  const url = useMemo(() => findFirstUrl(text), [text]);
  const [preview, setPreview] = useState<LinkPreview | null>(null);

  useEffect(() => {
    setPreview(null);
    if (url === null) {
      return undefined;
    }
    const token = getAccessToken();
    if (token === null) {
      return undefined;
    }

    let cancelled = false;
    client.fetchLinkPreview(token, url).then(
      (result) => {
        if (!cancelled) {
          setPreview(result);
        }
      },
      () => {
        if (!cancelled) {
          setPreview({ available: false });
        }
      },
    );
    return () => {
      cancelled = true;
    };
  }, [url, client, getAccessToken]);

  if (url === null || preview === null || !preview.available) {
    return null;
  }

  return (
    <a href={url} target="_blank" rel="noreferrer noopener" className={styles.card}>
      {/* alt="" — decorative thumbnail; the title/domain text right next to it already says everything a screen reader needs. */}
      {preview.imageUrl !== "" && <img src={preview.imageUrl} alt="" className={styles.image} />}
      <span className={styles.text}>
        {preview.title !== "" && <span className={styles.title}>{preview.title}</span>}
        {preview.description !== "" && <span className={styles.description}>{preview.description}</span>}
        <span className={styles.domain}>{extractDomain(url)}</span>
      </span>
    </a>
  );
}
