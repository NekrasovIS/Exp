import { act, renderHook, waitFor } from "@testing-library/react";
import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";

import { useVoiceRecorder } from "../../src/chat/useVoiceRecorder.js";

// jsdom implements neither MediaRecorder nor getUserMedia — this fake
// mirrors just enough of the real API (see MDN's MediaRecorder) for
// useVoiceRecorder.ts's own usage: a single ondataavailable delivery
// followed by a "stop" event, same shape CallPanel.test.tsx's
// fakeMediaStream() stands in for getUserMedia's real MediaStream.
class FakeMediaRecorder {
  static isTypeSupported = vi.fn((type: string) => type === "audio/webm;codecs=opus");

  ondataavailable: ((event: { data: Blob }) => void) | null = null;
  readonly mimeType: string;
  readonly stream: MediaStream;
  private readonly stopListeners: Array<() => void> = [];

  constructor(stream: MediaStream, options?: { mimeType?: string }) {
    this.stream = stream;
    this.mimeType = options?.mimeType ?? "audio/webm";
  }

  addEventListener(event: string, listener: () => void): void {
    if (event === "stop") {
      this.stopListeners.push(listener);
    }
  }

  start(): void {
    // no-op — a real recorder would begin buffering audio here.
  }

  stop(): void {
    this.ondataavailable?.({ data: new Blob(["fake-audio"], { type: this.mimeType }) });
    this.stopListeners.forEach((listener) => listener());
  }
}

function fakeMediaStream(stopTrack: () => void): MediaStream {
  const track = { kind: "audio", stop: stopTrack };
  return { getTracks: () => [track] } as unknown as MediaStream;
}

beforeEach(() => {
  vi.stubGlobal("MediaRecorder", FakeMediaRecorder);
});

afterEach(() => {
  vi.unstubAllGlobals();
});

describe("useVoiceRecorder", () => {
  it("opens the mic and starts recording", async () => {
    const getUserMedia = vi.fn().mockResolvedValue(fakeMediaStream(vi.fn()));
    Object.defineProperty(navigator, "mediaDevices", { configurable: true, value: { getUserMedia } });

    const { result } = renderHook(() => useVoiceRecorder());
    await act(() => result.current.start());

    expect(getUserMedia).toHaveBeenCalledWith({ audio: true });
    expect(result.current.isRecording).toBe(true);
  });

  it("stopping without ever starting resolves to null", async () => {
    const { result } = renderHook(() => useVoiceRecorder());
    await expect(result.current.stop()).resolves.toBeNull();
  });

  it("stop() releases the mic and resolves the recorded blob with a voice-message filename", async () => {
    const stopTrack = vi.fn();
    Object.defineProperty(navigator, "mediaDevices", {
      configurable: true,
      value: { getUserMedia: vi.fn().mockResolvedValue(fakeMediaStream(stopTrack)) },
    });

    const { result } = renderHook(() => useVoiceRecorder());
    await act(() => result.current.start());

    let recorded: Awaited<ReturnType<typeof result.current.stop>> = null;
    await act(async () => {
      recorded = await result.current.stop();
    });

    expect(stopTrack).toHaveBeenCalled();
    expect(result.current.isRecording).toBe(false);
    expect(recorded).not.toBeNull();
    expect(recorded?.filename).toMatch(/^voice-message-\d+\.webm$/);
    expect(recorded?.blob.type).toBe("audio/webm;codecs=opus");
  });

  it("reports an error when the microphone can't be opened", async () => {
    Object.defineProperty(navigator, "mediaDevices", {
      configurable: true,
      value: { getUserMedia: vi.fn().mockRejectedValue(new Error("denied")) },
    });

    const { result } = renderHook(() => useVoiceRecorder());
    await act(() => result.current.start());

    await waitFor(() => expect(result.current.error).toMatch(/microphone/i));
    expect(result.current.isRecording).toBe(false);
  });
});
