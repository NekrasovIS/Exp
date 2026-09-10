// Issue #308/#338/#340 — pinned messages for one channel: the full
// list (for the panel/count) plus a derived id set (for MessageList's
// inline "📌 Pinned" badge). Unlike useMessages()'s edit/delete, a
// `messagePinned` event triggers a full REST refetch rather than
// patching local state from the event's own fields — it only carries
// {id, pinnedBy, pinnedAt}, not the full message content the panel
// needs to render a new entry. `messageUnpinned` removes locally
// instead, since no new content is ever needed for that direction.

import { ChatRestClient } from "@devicehub/core";
import type { ChatClient, PinnedMessageInfo } from "@devicehub/core";
import { useCallback, useEffect, useMemo, useState } from "react";

import { chatServiceRestUrl } from "../config.js";
import { useSession } from "../session/SessionContext.js";

export function usePinnedMessages(channelId: number, socket: ChatClient) {
  const { getAccessToken } = useSession();
  const restClient = useMemo(() => new ChatRestClient(chatServiceRestUrl), []);
  const [pinned, setPinned] = useState<PinnedMessageInfo[]>([]);

  const refresh = useCallback(() => {
    const token = getAccessToken();
    if (token === null) {
      return;
    }
    restClient
      .listPinnedMessages(token, channelId)
      .then(setPinned)
      .catch(() => setPinned([]));
  }, [restClient, getAccessToken, channelId]);

  useEffect(() => {
    refresh();
  }, [refresh]);

  useEffect(() => {
    const offPinned = socket.on("messagePinned", () => refresh());
    const offUnpinned = socket.on("messageUnpinned", (id) => {
      setPinned((prev) => prev.filter((p) => p.id !== id));
    });
    return () => {
      offPinned();
      offUnpinned();
    };
  }, [socket, refresh]);

  const pinnedIds = useMemo(() => new Set(pinned.map((p) => p.id)), [pinned]);
  const pin = useCallback((id: number) => socket.sendPinMessage(id), [socket]);
  const unpin = useCallback((id: number) => socket.sendUnpinMessage(id), [socket]);

  return { pinned, pinnedIds, pin, unpin };
}
