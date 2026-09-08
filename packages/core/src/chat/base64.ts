// Dependency-free base64 encoder (issue #250) — chat-service's upload
// endpoint needs raw attachment bytes as base64 (`data_base64`, see
// ChatRestClient::uploadAttachment()). Deliberately not `btoa`/`Buffer`:
// this package targets browser, Node, and React Native alike, and none
// of those three globals is guaranteed present on all three without an
// extra runtime check this small a helper doesn't need.

const kAlphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

export function toBase64(bytes: Uint8Array): string {
  let result = "";
  let i = 0;
  for (; i + 3 <= bytes.length; i += 3) {
    const chunk = ((bytes[i] as number) << 16) | ((bytes[i + 1] as number) << 8) | (bytes[i + 2] as number);
    result += kAlphabet[(chunk >> 18) & 0x3f];
    result += kAlphabet[(chunk >> 12) & 0x3f];
    result += kAlphabet[(chunk >> 6) & 0x3f];
    result += kAlphabet[chunk & 0x3f];
  }
  const remaining = bytes.length - i;
  if (remaining === 1) {
    const chunk = (bytes[i] as number) << 16;
    result += kAlphabet[(chunk >> 18) & 0x3f];
    result += kAlphabet[(chunk >> 12) & 0x3f];
    result += "==";
  } else if (remaining === 2) {
    const chunk = ((bytes[i] as number) << 16) | ((bytes[i + 1] as number) << 8);
    result += kAlphabet[(chunk >> 18) & 0x3f];
    result += kAlphabet[(chunk >> 12) & 0x3f];
    result += kAlphabet[(chunk >> 6) & 0x3f];
    result += "=";
  }
  return result;
}
