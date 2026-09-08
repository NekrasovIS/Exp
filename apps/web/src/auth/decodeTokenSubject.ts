// Issue #267 — the UI needs to know the signed-in user's own login
// (e.g. to tell "my message" apart from someone else's, for edit/
// delete rights) without a round trip to /auth/verify on every sign-in
// or page load. TokenService.cpp's own format is
// base64url(json payload) + "." + base64url(HMAC) with payload
// `{"sub": subject, "exp": expiresAt}` (see its issueTokenInternal()) —
// the payload is signed, not encrypted, so reading it client-side is
// safe for display purposes. This is never used as an auth decision:
// every REST/WebSocket call still sends the raw token and the server
// verifies the signature itself; a tampered token here would just show
// the wrong name in the UI, not grant anything.
export function decodeTokenSubject(token: string): string | null {
  const payloadSegment = token.split(".")[0];
  if (payloadSegment === undefined) {
    return null;
  }
  try {
    const unpadded = payloadSegment.replaceAll("-", "+").replaceAll("_", "/");
    const base64 = unpadded + "=".repeat((4 - (unpadded.length % 4)) % 4);
    const json = atob(base64);
    const payload = JSON.parse(json) as { sub?: unknown };
    return typeof payload.sub === "string" ? payload.sub : null;
  } catch {
    return null;
  }
}
