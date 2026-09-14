// Issue #410 — the actual per-message wrapper shape
// (<li>/<div class=bubble>/<strong class=author>) was hand-copied
// independently by MessageList.tsx (channel chat) and
// DirectMessageList.tsx (DM) — the latter even imported MessageList's
// own CSS module directly just to reuse these classes, an awkward
// cross-feature coupling this removes. Everything beyond author/
// own-message styling (edit/delete/pin/reactions/reply for channel
// messages; nothing extra for DM) stays owned by each caller as
// `children` — this component only knows about the shape every message
// row shares, not what channel messages can do that DM messages can't.

import type { ReactNode } from "react";

import styles from "./MessageRow.module.css";

interface MessageRowProps {
  /** Anchor for scrollToMessage()-style deep links (issue #308/#338) —
   * only MessageList's channel messages need one; DM has no such
   * feature, so DirectMessageList omits it. */
  id?: string;
  isOwn: boolean;
  author: string;
  children: ReactNode;
}

export function MessageRow({ id, isOwn, author, children }: MessageRowProps) {
  return (
    <li id={id} className={`${styles.row} ${isOwn ? styles.rowOwn : ""}`}>
      <div className={`${styles.bubble} ${isOwn ? styles.bubbleOwn : ""}`}>
        <strong className={styles.author}>{author}</strong>
        {children}
      </div>
    </li>
  );
}
