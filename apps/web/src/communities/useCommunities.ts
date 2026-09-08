// Issue #266 — loads the signed-in user's communities and exposes
// joining one by invite code, mirroring DeviceHub's CommunitiesPanel:
// GET /communities/mine only (never the unfiltered /communities list —
// see README's own note on this), plus join-by-code.

import { ChatRestClient } from "@devicehub/core";
import type { ChatItem } from "@devicehub/core";
import { useCallback, useEffect, useMemo, useState } from "react";

import { chatServiceRestUrl } from "../config.js";
import { useSession } from "../session/SessionContext.js";

export function useCommunities() {
  const { getAccessToken } = useSession();
  const client = useMemo(() => new ChatRestClient(chatServiceRestUrl), []);

  const [communities, setCommunities] = useState<ChatItem[]>([]);
  const [error, setError] = useState<string | null>(null);
  const [loading, setLoading] = useState(true);

  const refresh = useCallback(async () => {
    const token = getAccessToken();
    if (token === null) {
      return;
    }
    setLoading(true);
    setError(null);
    try {
      setCommunities(await client.listCommunities(token));
    } catch {
      setError("Couldn't load your communities.");
    } finally {
      setLoading(false);
    }
  }, [client, getAccessToken]);

  useEffect(() => {
    void refresh();
  }, [refresh]);

  const joinByCode = useCallback(
    async (code: string) => {
      const token = getAccessToken();
      if (token === null) {
        return;
      }
      await client.joinCommunityByCode(token, code);
      await refresh();
    },
    [client, getAccessToken, refresh],
  );

  return { communities, loading, error, refresh, joinByCode };
}
