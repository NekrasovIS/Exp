// Live-message WebSocket lifecycle, shared by every screen that needs
// it (issue #264) — #267 (channel chat) and #268 (direct messages) each
// use this instead of managing a ChatClient/effect themselves. One
// instance subscribes to exactly one channel OR DM thread at a time
// (ChatClient's own constraint) — pass whichever id applies and leave
// the other undefined.

import { ChatClient } from "@devicehub/core";
import { useEffect, useMemo } from "react";

import { browserWebSocketFactory } from "./browserWebSocketFactory.js";
import { chatServiceWsUrl } from "../config.js";
import { useSession } from "../session/SessionContext.js";

export interface ChatSocketTarget {
  channelId?: number;
  dmThreadId?: number;
}

/** Constructs a ChatClient wired to the browser's native WebSocket and
 * connects it to @p target for as long as the calling component is
 * mounted with a valid target — reconnects if the target changes,
 * disconnects on unmount. Returns the client so callers can attach
 * `.on(...)` listeners and call its send*() methods. */
export function useChatSocket(target: ChatSocketTarget): ChatClient {
  const { getAccessToken } = useSession();

  const client = useMemo(() => new ChatClient(chatServiceWsUrl, browserWebSocketFactory), []);

  useEffect(() => {
    const token = getAccessToken();
    if (token === null) {
      return;
    }
    if (target.dmThreadId !== undefined) {
      client.connectToDirectMessageThread(token, target.dmThreadId);
    } else if (target.channelId !== undefined) {
      client.connectToChannel(token, target.channelId);
    } else {
      return;
    }
    return () => client.disconnect();
  }, [client, getAccessToken, target.channelId, target.dmThreadId]);

  return client;
}
