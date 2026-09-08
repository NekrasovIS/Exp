// Issue #268 — friend list, incoming requests, and managing them, via
// UserServiceClient (issue #249). Mirrors DeviceHub's FriendsPanel data
// needs; user-service, not chat-service, owns all of this (issue #187
// Phase 1).

import { UserServiceClient } from "@devicehub/core";
import type { FriendRequestInfo } from "@devicehub/core";
import { useCallback, useEffect, useMemo, useState } from "react";

import { userServiceUrl } from "../config.js";
import { useSession } from "../session/SessionContext.js";

export function useFriends() {
  const { getAccessToken } = useSession();
  const client = useMemo(() => new UserServiceClient(userServiceUrl), []);

  const [friends, setFriends] = useState<string[]>([]);
  const [incomingRequests, setIncomingRequests] = useState<FriendRequestInfo[]>([]);
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
      const [friendLogins, requests] = await Promise.all([
        client.listFriends(token),
        client.listIncomingFriendRequests(token),
      ]);
      setFriends(friendLogins);
      setIncomingRequests(requests);
    } catch {
      setError("Couldn't load your friends.");
    } finally {
      setLoading(false);
    }
  }, [client, getAccessToken]);

  useEffect(() => {
    void refresh();
  }, [refresh]);

  const sendRequest = useCallback(
    async (recipientLogin: string) => {
      const token = getAccessToken();
      if (token === null) {
        return;
      }
      await client.sendFriendRequest(token, recipientLogin);
      await refresh();
    },
    [client, getAccessToken, refresh],
  );

  const acceptRequest = useCallback(
    async (requestId: number) => {
      const token = getAccessToken();
      if (token === null) {
        return;
      }
      await client.acceptFriendRequest(token, requestId);
      await refresh();
    },
    [client, getAccessToken, refresh],
  );

  const declineRequest = useCallback(
    async (requestId: number) => {
      const token = getAccessToken();
      if (token === null) {
        return;
      }
      await client.declineFriendRequest(token, requestId);
      await refresh();
    },
    [client, getAccessToken, refresh],
  );

  const removeFriend = useCallback(
    async (login: string) => {
      const token = getAccessToken();
      if (token === null) {
        return;
      }
      await client.removeFriend(token, login);
      await refresh();
    },
    [client, getAccessToken, refresh],
  );

  return {
    friends,
    incomingRequests,
    loading,
    error,
    sendRequest,
    acceptRequest,
    declineRequest,
    removeFriend,
  };
}
