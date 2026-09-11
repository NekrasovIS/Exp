// Issue #322 — member list for a community, mirroring useIsModerator.ts's
// own shape (REST fetch via ChatRestClient.listMembers(), refetch on
// communityId change). Presence itself isn't handled here — it rides
// whichever channel's ChatClient socket happens to be subscribed
// (see ChatView's onOnlineMembers/onPresenceChanged props), same
// limitation DeviceHub's own MemberListPanel has: presence only starts
// flowing once a channel in this community is open.

import { ChatRestClient } from "@devicehub/core";
import { useEffect, useMemo, useState } from "react";

import { chatServiceRestUrl } from "../config.js";
import { useSession } from "../session/SessionContext.js";

export function useMembers(communityId: number | null) {
  const { getAccessToken } = useSession();
  const client = useMemo(() => new ChatRestClient(chatServiceRestUrl), []);
  const [members, setMembers] = useState<string[]>([]);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);

  useEffect(() => {
    const token = getAccessToken();
    if (token === null || communityId === null) {
      setMembers([]);
      return;
    }
    let cancelled = false;
    setLoading(true);
    setError(null);
    client
      .listMembers(token, communityId)
      .then((logins) => {
        if (!cancelled) {
          setMembers(logins);
        }
      })
      .catch(() => {
        if (!cancelled) {
          setError("Couldn't load members for this community.");
        }
      })
      .finally(() => {
        if (!cancelled) {
          setLoading(false);
        }
      });
    return () => {
      cancelled = true;
    };
  }, [client, getAccessToken, communityId]);

  return { members, loading, error };
}
