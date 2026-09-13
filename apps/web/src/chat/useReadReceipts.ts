// Issue #380 — "Seen by" indicators: fetches the initial snapshot of
// every member's read pointer for one channel/DM thread, then keeps it
// live via the same socket's `readReceiptChanged` event. Never computes
// "who has seen message X" itself — that's left to the caller, which
// checks `readPointers.get(login) >= message.id` per own message it
// renders (see MessageList.tsx/DirectMessageList.tsx), the same
// "server sends raw pointers, client derives 'seen'" split chat-service
// itself uses (explicit performance concern in the issue for large
// channels — recomputing per message on the server doesn't scale).

import { ChatRestClient } from "@devicehub/core";
import type { ChatClient } from "@devicehub/core";
import { useCallback, useEffect, useMemo, useState } from "react";

import type { ChatSocketTarget } from "./useChatSocket.js";
import { chatServiceRestUrl } from "../config.js";
import { useSession } from "../session/SessionContext.js";

export function useReadReceipts(target: ChatSocketTarget, socket: ChatClient) {
  const { getAccessToken } = useSession();
  const restClient = useMemo(() => new ChatRestClient(chatServiceRestUrl), []);
  const [readPointers, setReadPointers] = useState<ReadonlyMap<string, number>>(new Map());

  const refresh = useCallback(() => {
    const token = getAccessToken();
    if (token === null) {
      return;
    }
    const fetchReceipts =
      target.dmThreadId !== undefined
        ? restClient.fetchDmThreadReadReceipts(token, target.dmThreadId)
        : target.channelId !== undefined
          ? restClient.fetchChannelReadReceipts(token, target.channelId)
          : undefined;
    fetchReceipts
      ?.then((receipts) => setReadPointers(new Map(receipts.map((r) => [r.login, r.lastReadMessageId]))))
      .catch(() => setReadPointers(new Map()));
  }, [restClient, getAccessToken, target.channelId, target.dmThreadId]);

  useEffect(() => {
    refresh();
  }, [refresh]);

  useEffect(() => {
    return socket.on("readReceiptChanged", (login, lastReadMessageId) => {
      setReadPointers((prev) => {
        // Mirrors the server's own GREATEST() — an out-of-order/stale
        // event (possible over a WebSocket the same way an HTTP
        // response can arrive late) must never move a pointer backward.
        if (lastReadMessageId <= (prev.get(login) ?? -1)) {
          return prev;
        }
        return new Map(prev).set(login, lastReadMessageId);
      });
    });
  }, [socket]);

  return { readPointers };
}
