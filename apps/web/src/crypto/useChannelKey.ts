// Issue #269 — fetches and unwraps this login's own copy of an
// encrypted channel's symmetric key (mirrors MainWindow.cpp's
// myChannelKeyFetched/myChannelKeyNotFound handling). null while
// loading; stays null (with hasAccess=false) if the server has no
// wrapped key on file for this login yet, or if unwrapping fails.

import { ChatRestClient } from "@devicehub/core";
import { useEffect, useState } from "react";

import { unwrapKey } from "./channelCrypto.js";
import { type IdentityKeys } from "./useIdentityKeys.js";
import { chatServiceRestUrl } from "../config.js";
import { useSession } from "../session/SessionContext.js";

export interface ChannelKeyState {
  loading: boolean;
  /** True only once a key was successfully fetched *and* unwrapped —
   * distinguishing "no key on file yet" from "fetch/unwrap failed" is
   * display-only here (both show as no access), matching how
   * MainWindow.cpp treats them the same way in the chat UI. */
  channelKey: Uint8Array | null;
}

export function useChannelKey(channelId: number, identityKeys: IdentityKeys | null): ChannelKeyState {
  const { getAccessToken } = useSession();
  const [state, setState] = useState<ChannelKeyState>({ loading: true, channelKey: null });

  useEffect(() => {
    setState({ loading: true, channelKey: null });
    const token = getAccessToken();
    if (token === null || identityKeys === null) {
      return;
    }
    let cancelled = false;
    void (async () => {
      const client = new ChatRestClient(chatServiceRestUrl);
      let wrappedKey: string | null;
      try {
        wrappedKey = await client.fetchMyChannelKey(token, channelId);
      } catch {
        wrappedKey = null;
      }
      if (wrappedKey === null) {
        if (!cancelled) {
          setState({ loading: false, channelKey: null });
        }
        return;
      }
      const unwrapped = await unwrapKey(wrappedKey, identityKeys.ownPublicKey, identityKeys.ownSecretKey);
      if (!cancelled) {
        setState({ loading: false, channelKey: unwrapped });
      }
    })();
    return () => {
      cancelled = true;
    };
  }, [getAccessToken, channelId, identityKeys]);

  return state;
}
