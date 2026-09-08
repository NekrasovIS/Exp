// Shared test helpers — mirrors packages/core's own test convention for
// stubbing fetch, since every component here constructs its own REST
// client internally (using the global fetch) rather than taking one
// injected.

export function jsonResponse(status: number, body: unknown): Response {
  return new Response(JSON.stringify(body), { status });
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
