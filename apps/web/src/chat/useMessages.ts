// Issue #267 — message history + live updates for one channel.
// listMessages() returns a page in chronological (oldest-first) order,
// ending right before `beforeId` (see README's own description of the
// route) — loadOlder() re-uses the oldest currently-loaded message's id
// as the next beforeId and prepends the result, so the combined list
// stays chronological throughout.

import { ChatRestClient } from "@devicehub/core";
import type { ChatMessageInfo } from "@devicehub/core";
import { useCallback, useEffect, useMemo, useState } from "react";

import { useChatSocket } from "./useChatSocket.js";
import { chatServiceRestUrl } from "../config.js";
import { useSession } from "../session/SessionContext.js";

const kPageSize = 50;

export function useMessages(channelId: number) {
  const { getAccessToken } = useSession();
  const restClient = useMemo(() => new ChatRestClient(chatServiceRestUrl), []);
  const socket = useChatSocket({ channelId });

  const [messages, setMessages] = useState<ChatMessageInfo[]>([]);
  // ChatMessageInfo (packages/core) has no editedAt field — the REST
  // history endpoint doesn't return one, only the live
  // message_edited WebSocket event does. Tracked separately here
  // rather than widening that shared type just for this one flag.
  const [editedIds, setEditedIds] = useState<ReadonlySet<number>>(new Set());
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
      .listMessages(token, channelId, kPageSize)
      .then((page) => {
        setMessages(page);
        setHasMore(page.length === kPageSize);
      })
      .catch(() => setError("Couldn't load messages for this channel."))
      .finally(() => setLoading(false));
  }, [restClient, getAccessToken, channelId]);

  useEffect(() => {
    const offMessage = socket.on("message", (message) => {
      setMessages((prev) => [...prev, message]);
    });
    const offEdited = socket.on("messageEdited", (id, newBody) => {
      setMessages((prev) => prev.map((m) => (m.id === id ? { ...m, body: newBody } : m)));
      setEditedIds((prev) => new Set(prev).add(id));
    });
    const offDeleted = socket.on("messageDeleted", (id) => {
      setMessages((prev) => prev.filter((m) => m.id !== id));
    });
    return () => {
      offMessage();
      offEdited();
      offDeleted();
    };
  }, [socket]);

  const loadOlder = useCallback(async () => {
    const token = getAccessToken();
    const oldest = messages[0];
    if (token === null || oldest === undefined || !hasMore) {
      return;
    }
    try {
      const page = await restClient.listMessages(token, channelId, kPageSize, oldest.id);
      setMessages((prev) => [...page, ...prev]);
      setHasMore(page.length === kPageSize);
    } catch {
      setError("Couldn't load older messages.");
    }
  }, [restClient, getAccessToken, channelId, messages, hasMore]);

  const sendMessage = useCallback(
    (body: string, attachmentId?: number) => socket.sendMessage(body, attachmentId),
    [socket],
  );
  const editMessage = useCallback(
    (id: number, newBody: string) => socket.sendEditMessage(id, newBody),
    [socket],
  );
  const deleteMessage = useCallback((id: number) => socket.sendDeleteMessage(id), [socket]);

  return { messages, editedIds, loading, error, hasMore, loadOlder, sendMessage, editMessage, deleteMessage };
}
