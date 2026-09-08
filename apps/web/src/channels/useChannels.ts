// Issue #266 — loads the channels of a given community and exposes
// creating a new one, mirroring DeviceHub's ChannelsPanel. Returns an
// empty list (not an error) while @p communityId is null, so callers
// don't need to guard rendering on it themselves.

import { ChatRestClient } from "@devicehub/core";
import type { ChatItem } from "@devicehub/core";
import { useCallback, useEffect, useMemo, useState } from "react";

import { chatServiceRestUrl } from "../config.js";
import { useSession } from "../session/SessionContext.js";

export function useChannels(communityId: number | null) {
  const { getAccessToken } = useSession();
  const client = useMemo(() => new ChatRestClient(chatServiceRestUrl), []);

  const [channels, setChannels] = useState<ChatItem[]>([]);
  const [error, setError] = useState<string | null>(null);
  const [loading, setLoading] = useState(false);

  const refresh = useCallback(async () => {
    const token = getAccessToken();
    if (token === null || communityId === null) {
      setChannels([]);
      return;
    }
    setLoading(true);
    setError(null);
    try {
      setChannels(await client.listChannels(token, communityId));
    } catch {
      setError("Couldn't load channels for this community.");
    } finally {
      setLoading(false);
    }
  }, [client, getAccessToken, communityId]);

  useEffect(() => {
    void refresh();
  }, [refresh]);

  const createChannel = useCallback(
    // Issue #269: returns the created channel's id/isEncrypted (not
    // void) so an encrypted creation can chain the key-generation/
    // wrap-for-members step (useEncryptedChannelSetup) — that step
    // needs the real channel id, which only exists once this call
    // resolves.
    async (name: string, isEncrypted = false) => {
      const token = getAccessToken();
      if (token === null || communityId === null) {
        return null;
      }
      const created = await client.createChannel(token, communityId, name, isEncrypted);
      await refresh();
      return created;
    },
    [client, getAccessToken, communityId, refresh],
  );

  return { channels, loading, error, refresh, createChannel };
}
