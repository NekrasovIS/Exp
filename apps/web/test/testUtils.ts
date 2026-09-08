// Shared test helpers — mirrors packages/core's own test convention for
// stubbing fetch, since every component here constructs its own REST
// client internally (using the global fetch) rather than taking one
// injected.

import { vi } from "vitest";

export function jsonResponse(status: number, body: unknown): Response {
  return new Response(JSON.stringify(body), { status });
}

/** A `fetch` stub that dispatches by URL pattern instead of call order —
 * needed once a component/hook combination fires several requests whose
 * relative order isn't part of what the test cares about (e.g. issue
 * #269's identity-key publish racing the channel-key fetch). Any URL
 * matching no route gets a bare `jsonResponse(200, [])`, which is enough
 * for routes a given test doesn't care about (list endpoints tolerate an
 * empty array; anything that checks its result explicitly should get its
 * own route instead of relying on this). */
export function routedFetch(routes: Array<[RegExp, () => Response]>) {
  return vi.fn(async (url: string) => {
    const route = routes.find(([pattern]) => pattern.test(url));
    return route !== undefined ? route[1]() : jsonResponse(200, []);
  });
}

export function encodePayloadBase64Url(payload: unknown): string {
  const base64 = btoa(JSON.stringify(payload));
  return base64.replaceAll("+", "-").replaceAll("/", "_").replace(/=+$/, "");
}

/** A session-storage token whose payload segment decodeTokenSubject()
 * can read back as @p login — matches TokenService.cpp's format closely
 * enough for tests (see decodeTokenSubject.ts's own doc comment); the
 * signature segment is never checked client-side. */
export function fakeToken(login: string): string {
  return `${encodePayloadBase64Url({ sub: login, exp: 9999999999 })}.sig`;
}

// jsdom doesn't implement WebSocket at all — useChatSocket's default
// factory needs *some* global to construct against, so this stands in
// for it (issue #264/#267). Only the surface ChatClient actually
// touches (WebSocketLike) is implemented; `stubGlobal("WebSocket",
// FakeWebSocket)` in a test's beforeEach, then read/drive
// FakeWebSocket.instances to observe what ChatClient did.
export class FakeWebSocket {
  static instances: FakeWebSocket[] = [];
  closed = false;
  sent: string[] = [];
  onopen: (() => void) | null = null;
  onclose: (() => void) | null = null;
  onerror: (() => void) | null = null;
  onmessage: ((event: { data: unknown }) => void) | null = null;

  constructor(public readonly url: string) {
    FakeWebSocket.instances.push(this);
  }

  send(data: string): void {
    this.sent.push(data);
  }

  close(): void {
    this.closed = true;
  }
}
