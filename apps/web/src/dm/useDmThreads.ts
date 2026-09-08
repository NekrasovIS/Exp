// Issue #268 — the list of existing DM threads (chat-service, issue
// #187 Phase 2), so a conversation can be reopened directly rather
// than only via FriendsPanel's "Message" button each time.

import { ChatRestClient } from "@devicehub/core";
import type { DirectMessageThreadInfo } from "@devicehub/core";
import { useCallback, useEffect, useMemo, useState } from "react";

import { chatServiceRestUrl } from "../config.js";
import { useSession } from "../session/SessionContext.js";

export function useDmThreads() {
  const { getAccessToken } = useSession();
  const client = useMemo(() => new ChatRestClient(chatServiceRestUrl), []);

  const [threads, setThreads] = useState<DirectMessageThreadInfo[]>([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);

  const refresh = useCallback(async () => {
    const token = getAccessToken();
    if (token === null) {
      return;
    }
    setLoading(true);
    setError(null);
    try {
      setThreads(await client.listDmThreads(token));
    } catch {
      setError("Couldn't load your conversations.");
    } finally {
      setLoading(false);
    }
  }, [client, getAccessToken]);

  useEffect(() => {
    void refresh();
  }, [refresh]);

  const openThreadWith = useCallback(
    async (recipientLogin: string): Promise<number | null> => {
      const token = getAccessToken();
      if (token === null) {
        return null;
      }
      const { id } = await client.openDmThread(token, recipientLogin);
      await refresh();
      return id;
    },
    [client, getAccessToken, refresh],
  );

  return { threads, loading, error, openThreadWith };
}
