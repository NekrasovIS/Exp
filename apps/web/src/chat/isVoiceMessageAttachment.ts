// Issue #360 — web analog of DeviceHub's isAudioAttachment()
// (ChatMessageRow.cpp): renders a voice-message player instead of a
// "Download" link for any attachment whose filename ends in one of
// these extensions. Same accepted trade-off as desktop's own doc
// comment describes for `.wav`: there is no way to tell a real voice
// message apart from an arbitrary uploaded file with the same
// extension from the filename alone, so every match is treated as one.
// This is safe on the web client specifically because, unlike desktop,
// it has no competing image/video-preview feature that also claims one
// of these extensions (`.webm` in particular) — see the desktop
// isAudioAttachment() doc comment for why *that* side needs an extra
// filename-prefix guard before recognizing `.webm` as audio.
//
// `.wav` is desktop's own recording format. `.webm`/`.ogg`/`.m4a` cover
// what this client's own MediaRecorder produces (container choice is
// browser-dependent — see useVoiceRecorder.ts) and, as a side effect,
// let a web-recorded voice message be recognized as one on desktop too
// (best-effort: actual playback there still depends on which codecs Qt
// Multimedia has available).
const kVoiceMessageExtensions = [".wav", ".webm", ".ogg", ".m4a"];

export function isVoiceMessageAttachment(filename: string): boolean {
  const lower = filename.toLowerCase();
  return kVoiceMessageExtensions.some((extension) => lower.endsWith(extension));
}
