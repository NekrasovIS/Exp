import { describe, expect, it } from "vitest";

import { isVoiceMessageAttachment } from "../../src/chat/isVoiceMessageAttachment.js";

describe("isVoiceMessageAttachment", () => {
  it("recognizes every extension a voice-message recording can use", () => {
    expect(isVoiceMessageAttachment("voice-message-123.wav")).toBe(true);
    expect(isVoiceMessageAttachment("voice-message-123.webm")).toBe(true);
    expect(isVoiceMessageAttachment("voice-message-123.ogg")).toBe(true);
    expect(isVoiceMessageAttachment("voice-message-123.m4a")).toBe(true);
    expect(isVoiceMessageAttachment("VOICE-MESSAGE-123.WEBM")).toBe(true);
  });

  it("rejects filenames with an unrelated extension", () => {
    expect(isVoiceMessageAttachment("report.pdf")).toBe(false);
    expect(isVoiceMessageAttachment("photo.png")).toBe(false);
  });
});
