// Issue #267 — whether the signed-in user moderates @p communityId,
// needed to decide whether they can delete someone else's message
// (author-only for edits, author-or-moderator for deletes — see
// README's chat-service section).

import { ChatRestClient } from "@devicehub/core";
import { useEffect, useMemo, useState } from "react";

import { chatServiceRestUrl } from "../config.js";
import { useSession } from "../session/SessionContext.js";

export function useIsModerator(communityId: number | null): boolean {
  const { getAccessToken, currentLogin } = useSession();
  const client = useMemo(() => new ChatRestClient(chatServiceRestUrl), []);
  const [isModerator, setIsModerator] = useState(false);

  useEffect(() => {
    const token = getAccessToken();
    if (token === null || communityId === null || currentLogin === null) {
      setIsModerator(false);
      return;
    }
    let cancelled = false;
    client
      .listModerators(token, communityId)
      .then((moderators) => {
        if (!cancelled) {
          setIsModerator(moderators.includes(currentLogin));
        }
      })
      .catch(() => {
        if (!cancelled) {
          setIsModerator(false);
        }
      });
    return () => {
      cancelled = true;
    };
  }, [client, getAccessToken, communityId, currentLogin]);

  return isModerator;
}
