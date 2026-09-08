// Issue #268 — message history + live updates for one DM thread.
// Mirrors useMessages (channel chat, #267) minus what DM threads don't
// have: no edit/delete/search/attachments (see README's description of
// Phase 2b — "no call/typing/edit/delete for DM threads"), and sending
// goes over the same live WebSocket connection as receiving (DeviceHub's
// own MainWindow uses ChatClient::sendMessage() for DMs too, not
// ChatRestClient::sendDirectMessage() — the REST method exists for API
// parity but isn't actually the send path).

import { ChatRestClient } from "@devicehub/core";
import type { DirectMessageInfo } from "@devicehub/core";
import { useCallback, useEffect, useMemo, useState } from "react";

import { useChatSocket } from "../chat/useChatSocket.js";
import { chatServiceRestUrl } from "../config.js";
import { useSession } from "../session/SessionContext.js";

const kPageSize = 50;

export function useDirectMessages(threadId: number) {
  const { getAccessToken } = useSession();
  const restClient = useMemo(() => new ChatRestClient(chatServiceRestUrl), []);
  const socket = useChatSocket({ dmThreadId: threadId });

  const [messages, setMessages] = useState<DirectMessageInfo[]>([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);
  const [hasMore, setHasMore] = useState(true);

  useEffect(() => {
    const token = getAccessToken();
    if (token === null) {
      return;
    }
    setMessages([]);
    setHasMore(true);
    setLoading(true);
    setError(null);
    restClient
      .listDirectMessages(token, threadId, kPageSize)
      .then((page) => {
        setMessages(page);
        setHasMore(page.length === kPageSize);
      })
      .catch(() => setError("Couldn't load this conversation."))
      .finally(() => setLoading(false));
  }, [restClient, getAccessToken, threadId]);

  useEffect(() => {
    return socket.on("message", (message) => {
      setMessages((prev) => [...prev, message]);
    });
  }, [socket]);

  const loadOlder = useCallback(async () => {
    const token = getAccessToken();
    const oldest = messages[0];
    if (token === null || oldest === undefined || !hasMore) {
      return;
    }
    try {
      const page = await restClient.listDirectMessages(token, threadId, kPageSize, oldest.id);
      setMessages((prev) => [...page, ...prev]);
      setHasMore(page.length === kPageSize);
    } catch {
      setError("Couldn't load older messages.");
    }
  }, [restClient, getAccessToken, threadId, messages, hasMore]);

  const sendMessage = useCallback((body: string) => socket.sendMessage(body), [socket]);

  return { messages, loading, error, hasMore, loadOlder, sendMessage };
}
