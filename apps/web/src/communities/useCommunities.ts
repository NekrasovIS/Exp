// Issue #266 — loads the signed-in user's communities and exposes
// joining one by invite code, mirroring DeviceHub's CommunitiesPanel:
// GET /communities/mine only (never the unfiltered /communities list —
// see README's own note on this), plus join-by-code.

import { ChatRestClient } from "@devicehub/core";
import type { ChatItem } from "@devicehub/core";
import { useCallback, useEffect, useMemo, useState } from "react";

import { takePendingInviteCode } from "./pendingInvite.js";
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

  // Issue #301 — owner-only (chat-service itself enforces this, see
  // ChatRestClient.regenerateInviteCode()'s doc comment); refreshes
  // afterward so CommunitiesSidebar's invite-link block picks up the
  // new code without a separate refetch call at the UI layer.
  const regenerateInviteCode = useCallback(
    async (communityId: number) => {
      const token = getAccessToken();
      if (token === null) {
        return;
      }
      await client.regenerateInviteCode(token, communityId);
      await refresh();
    },
    [client, getAccessToken, refresh],
  );

  // Issue #301 — consumes a code stashed by JoinPage's /join/:code
  // before redirecting an unauthenticated visitor to /login: this hook
  // (via CommunitiesMode) is what actually mounts once that visitor is
  // back on a signed-in HomePage, so this is where the deferred half of
  // that join happens. take() clears the stash — never re-applies it on
  // a later mount/refresh. A failed join (bad/expired code) is silently
  // dropped rather than surfaced through `error` — that field means
  // "couldn't load your communities", a different failure than "that
  // invite didn't work", and this isn't the screen that showed the
  // visitor the link in the first place.
  //
  // Gated on getAccessToken() !== null, not just "ran once on mount":
  // JoinPage itself calls this hook (for joinByCode) even on its own
  // not-yet-authenticated render, right where it also stashes the
  // code — an ungated take() here would consume and discard that same
  // code within the same tick, before the visitor ever reaches /login.
  useEffect(() => {
    if (getAccessToken() === null) {
      return;
    }
    const pendingCode = takePendingInviteCode();
    if (pendingCode !== null) {
      void joinByCode(pendingCode).catch(() => {});
    }
    // Deliberately not depending on joinByCode (its identity changes
    // with every refresh()) — this must run only once per real mount,
    // keyed on auth state, not on every callback identity change.
  }, [getAccessToken]);

  return { communities, loading, error, refresh, joinByCode, regenerateInviteCode };
}
