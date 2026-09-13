// Issue #396/#397 — link previews. There's no existing URL-detection
// anywhere in either client yet (MessageBody only highlights @mentions,
// message text otherwise renders as plain, non-clickable text), so this
// is a fresh, minimal regex rather than reuse of something that already
// makes links clickable.

const kUrlPattern = /https?:\/\/[^\s<>"')]+/;

/** First http(s) URL found in @p text, or null if none — trailing
 * punctuation a user would naturally type after a link ("check this
 * out: https://example.test." or "(https://example.test)") isn't
 * stripped; the tradeoff of an occasional trailing "." or ")" ending up
 * in the request is preferred over the complexity of guessing which
 * trailing characters were part of the URL versus sentence punctuation. */
export function findFirstUrl(text: string): string | null {
  const match = kUrlPattern.exec(text);
  return match?.[0] ?? null;
}
