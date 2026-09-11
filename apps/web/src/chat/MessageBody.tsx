// Issue #307 — highlights @mentions in a message body, the web
// equivalent of DeviceHub's message_formatting::highlightMentions()
// (src/ui/MessageFormatting.cpp). Builds actual React nodes rather than
// an HTML string, so this never touches dangerouslySetInnerHTML — the
// plain-text segments stay exactly as React-escaped as the single
// <span>{body}</span> this replaces.

import { Fragment } from "react";

import styles from "./MessageList.module.css";

// Same pattern as the C++ side's mentionPattern(): a negative lookbehind
// on \w/'.' before '@' so "bob@example.com" isn't split into a mention
// of "example". Global + capturing so String.split() below keeps the
// matched logins as their own array entries.
const kMentionPattern = /(?<![\w.])@([A-Za-z0-9_-]+)/g;

interface MessageBodyProps {
  text: string;
}

export function MessageBody({ text }: MessageBodyProps) {
  const parts = text.split(kMentionPattern);
  // String.split() with a capturing global regex alternates
  // [plain, captured-login, plain, captured-login, ...] — odd indices
  // are always the login (without its leading '@'), even indices are
  // the plain text around them (possibly empty).
  return (
    <>
      {parts.map((part, index) =>
        index % 2 === 1 ? (
          <strong key={index} className={styles.mention}>
            @{part}
          </strong>
        ) : (
          <Fragment key={index}>{part}</Fragment>
        ),
      )}
    </>
  );
}
