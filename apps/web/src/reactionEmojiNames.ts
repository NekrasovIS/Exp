// Issue #444 — shared between CallPanel.tsx's call-reaction buttons and
// MessageList.tsx's per-message reaction picker: both used the same
// fixed 5-emoji set, previously duplicated as their own local
// kCallReactionEmojis/kReactionEmojis constants, and neither gave their
// emoji-only buttons an accessible name — a screen reader announced
// either nothing useful or the raw Unicode codepoint.

/** The one fixed reaction set used everywhere in the web client —
 * matches DeviceHub's own ChatMessageRow::reactionEmojis() (issue #312),
 * so a user sees the same choices in every client and every reaction
 * surface (call reactions, message reactions). */
export const kReactionEmojis: readonly string[] = ["👍", "❤️", "😂", "🎉", "👏"];

const kEmojiNames: Readonly<Record<string, string>> = {
  "👍": "Thumbs up",
  "❤️": "Heart",
  "😂": "Laughing",
  "🎉": "Party popper",
  "👏": "Clapping hands",
};

/** Falls back to the emoji itself for one outside the fixed set above,
 * so a caller never renders an empty aria-label. */
export function reactionEmojiName(emoji: string): string {
  return kEmojiNames[emoji] ?? emoji;
}
