// Adapts the browser's native WebSocket to @devicehub/core's WebSocketLike
// (issue #264) — can't pass a WebSocket instance directly: its
// onopen/onclose/onerror callbacks take a DOM Event argument, while
// WebSocketLike's take none, and TypeScript's strict function-type
// variance on a mutable property (not method-shorthand) checks that
// bidirectionally, so a raw WebSocket doesn't structurally satisfy it.

import type { WebSocketLike } from "@devicehub/core";

export function browserWebSocketFactory(url: string): WebSocketLike {
  const socket = new WebSocket(url);
  const adapter: WebSocketLike = {
    send: (data: string) => socket.send(data),
    close: () => socket.close(),
    onopen: null,
    onclose: null,
    onerror: null,
    onmessage: null,
  };
  socket.onopen = () => adapter.onopen?.();
  socket.onclose = () => adapter.onclose?.();
  socket.onerror = () => adapter.onerror?.();
  socket.onmessage = (event) => adapter.onmessage?.({ data: event.data });
  return adapter;
}
