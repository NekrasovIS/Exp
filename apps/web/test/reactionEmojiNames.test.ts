import { describe, expect, it } from "vitest";

import { kReactionEmojis, reactionEmojiName } from "../src/reactionEmojiNames.js";

describe("reactionEmojiName", () => {
  it("names every emoji in the shared reaction set", () => {
    for (const emoji of kReactionEmojis) {
      expect(reactionEmojiName(emoji)).not.toBe(emoji);
      expect(reactionEmojiName(emoji).length).toBeGreaterThan(0);
    }
  });

  it("falls back to the emoji itself for one outside the fixed set", () => {
    expect(reactionEmojiName("🚀")).toBe("🚀");
  });
});
