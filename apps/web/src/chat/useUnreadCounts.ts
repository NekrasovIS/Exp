// Issue #310/#350 — web-client counterpart of DeviceHub's unread
// badges (issue #349), backed by the same chat-service counters
// (issue #348). Polls ChatRestClient::fetchUnreadCounts() periodically
// — there's no live WebSocket push of an updated count for a
// channel/thread nothing is currently subscribed to (chat-service only
// relays within an already-subscribed channel) — plus once immediately
// on mount and whenever the caller invokes refresh() (e.g. right after
// switching communities, mirroring MainWindow's own trigger points).
//
// clearChannelLocally()/clearThreadLocally() are the optimistic-UI half
// of the same story as DeviceHub's ChannelsPanel::setOpenChannelId():
// opening a channel/thread should drop its badge immediately, without
// waiting for the next poll tick or the mark-read round trip (that
// round trip itself lives in useMessages()/useDirectMessages(), which
// already own the "just loaded/received a message" moment needed to
// know which message id to mark read up to).

import { ChatRestClient } from "@devicehub/core";
import { useCallback, useEffect, useMemo, useState } from "react";

import { chatServiceRestUrl } from "../config.js";
import { useSession } from "../session/SessionContext.js";

const kPollIntervalMs = 30 * 1000;

function clearedLocally(counts: ReadonlyMap<number, number>, id: number): Map<number, number> | null {
  if ((counts.get(id) ?? 0) === 0) {
    return null; // Already zero — no state update, no extra re-render.
  }
  const next = new Map(counts);
  next.set(id, 0);
  return next;
}

export function useUnreadCounts() {
  const { getAccessToken } = useSession();
  const client = useMemo(() => new ChatRestClient(chatServiceRestUrl), []);

  const [channelCounts, setChannelCounts] = useState<ReadonlyMap<number, number>>(new Map());
  const [threadCounts, setThreadCounts] = useState<ReadonlyMap<number, number>>(new Map());

  const refresh = useCallback(async () => {
    const token = getAccessToken();
    if (token === null) {
      return;
    }
    try {
      const { channels, threads } = await client.fetchUnreadCounts(token);
      setChannelCounts(new Map(channels.map((entry) => [entry.channelId, entry.unreadCount])));
      setThreadCounts(new Map(threads.map((entry) => [entry.threadId, entry.unreadCount])));
    } catch {
      // Best-effort — badges simply don't refresh this cycle; the next
      // poll tick tries again.
    }
  }, [client, getAccessToken]);

  useEffect(() => {
    void refresh();
    const interval = setInterval(() => void refresh(), kPollIntervalMs);
    return () => clearInterval(interval);
  }, [refresh]);

  const clearChannelLocally = useCallback((channelId: number) => {
    setChannelCounts((prev) => clearedLocally(prev, channelId) ?? prev);
  }, []);
  const clearThreadLocally = useCallback((threadId: number) => {
    setThreadCounts((prev) => clearedLocally(prev, threadId) ?? prev);
  }, []);

  return { channelCounts, threadCounts, refresh, clearChannelLocally, clearThreadLocally };
}
