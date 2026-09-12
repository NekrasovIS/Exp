// Issue #360 — records a voice message via the browser's MediaRecorder
// API, the web analog of DeviceHub's VoiceMessageRecorder (own
// AudioInputDevice, independent of the shared one used by the mic test
// and calls — here that's `getUserMedia({ audio: true })` opening its
// own mic session, released again in stop()/discard()).
//
// Filenames use the same "voice-message-<epoch-ms>" prefix desktop's
// own recorder produces (MainWindow::onVoiceRecordToggled()) — matched
// by both isVoiceMessageAttachment.ts (web) and isAudioAttachment()
// (desktop) to recognize a recording made on either client.
import { useCallback, useRef, useState } from "react";

// Preference order: Opus-in-WebM is what Chrome/Firefox actually
// support for audio-only MediaRecorder capture; Ogg/MP4 are kept as
// fallbacks for engines that don't offer the WebM writer.
const kCandidateMimeTypes = ["audio/webm;codecs=opus", "audio/webm", "audio/ogg;codecs=opus", "audio/mp4"];

const kExtensionsByMimePrefix: Array<[string, string]> = [
  ["audio/webm", ".webm"],
  ["audio/ogg", ".ogg"],
  ["audio/mp4", ".m4a"],
];

function extensionForMimeType(mimeType: string): string {
  const found = kExtensionsByMimePrefix.find(([prefix]) => mimeType.startsWith(prefix));
  return found?.[1] ?? ".webm";
}

function pickSupportedMimeType(): string | undefined {
  return kCandidateMimeTypes.find((candidate) => MediaRecorder.isTypeSupported(candidate));
}

export interface RecordedVoiceMessage {
  blob: Blob;
  filename: string;
}

export interface VoiceRecorderState {
  isRecording: boolean;
  error: string | null;
  start: () => Promise<void>;
  stop: () => Promise<RecordedVoiceMessage | null>;
}

export function useVoiceRecorder(): VoiceRecorderState {
  const [isRecording, setIsRecording] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const recorderRef = useRef<MediaRecorder | null>(null);
  const chunksRef = useRef<Blob[]>([]);

  const start = useCallback(async () => {
    setError(null);
    try {
      const stream = await navigator.mediaDevices.getUserMedia({ audio: true });
      const mimeType = pickSupportedMimeType();
      const recorder =
        mimeType !== undefined ? new MediaRecorder(stream, { mimeType }) : new MediaRecorder(stream);
      chunksRef.current = [];
      recorder.ondataavailable = (event) => {
        if (event.data.size > 0) {
          chunksRef.current.push(event.data);
        }
      };
      recorder.start();
      recorderRef.current = recorder;
      setIsRecording(true);
    } catch {
      setError("Couldn't access the microphone.");
    }
  }, []);

  const stop = useCallback((): Promise<RecordedVoiceMessage | null> => {
    const recorder = recorderRef.current;
    if (recorder === null) {
      return Promise.resolve(null);
    }
    return new Promise((resolve) => {
      recorder.addEventListener(
        "stop",
        () => {
          recorder.stream.getTracks().forEach((track) => track.stop());
          recorderRef.current = null;
          setIsRecording(false);
          const mimeType = recorder.mimeType || "audio/webm";
          if (chunksRef.current.length === 0) {
            resolve(null);
            return;
          }
          const blob = new Blob(chunksRef.current, { type: mimeType });
          const filename = `voice-message-${Date.now()}${extensionForMimeType(mimeType)}`;
          resolve({ blob, filename });
        },
        { once: true },
      );
      recorder.stop();
    });
  }, []);

  return { isRecording, error, start, stop };
}
