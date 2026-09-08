// Issue #269 — the creation-time half of channel encryption, mirroring
// MainWindow.cpp's channelCreated handler + wrapPendingEncryptedChannelKeyForMember():
// generate a fresh channel key, wrap it for the creator's own identity
// immediately (no round trip needed, the public half is already known
// locally), then wrap it for every other current community member who
// has published a public key — one who hasn't is skipped (returned in
// skippedLogins) rather than blocking channel creation on it.

import { ChatRestClient, UserServiceClient } from "@devicehub/core";
import { useCallback, useMemo } from "react";

import { generateChannelKey, wrapKeyForRecipient } from "./channelCrypto.js";
import { getSodium } from "./sodium.js";
import { type IdentityKeys } from "./useIdentityKeys.js";
import { chatServiceRestUrl, userServiceUrl } from "../config.js";
import { useSession } from "../session/SessionContext.js";

export function useEncryptedChannelSetup() {
  const { getAccessToken, currentLogin } = useSession();
  const chatClient = useMemo(() => new ChatRestClient(chatServiceRestUrl), []);
  const userClient = useMemo(() => new UserServiceClient(userServiceUrl), []);

  const setUpEncryptedChannel = useCallback(
    async (channelId: number, communityId: number, identityKeys: IdentityKeys): Promise<string[]> => {
      const token = getAccessToken();
      if (token === null || currentLogin === null) {
        return [];
      }
      const sodium = await getSodium();
      const channelKey = await generateChannelKey();

      const ownWrapped = await wrapKeyForRecipient(channelKey, identityKeys.ownPublicKey);
      await chatClient.setChannelKey(token, channelId, currentLogin, ownWrapped);

      const members = await chatClient.listMembers(token, communityId);
      const skippedLogins: string[] = [];
      await Promise.all(
        members
          .filter((login) => login !== currentLogin)
          .map(async (login) => {
            const profile = await userClient.fetchProfile(token, login).catch(() => null);
            if (profile?.publicKey === undefined) {
              skippedLogins.push(login);
              return;
            }
            const recipientPublicKey = sodium.from_base64(profile.publicKey, sodium.base64_variants.ORIGINAL);
            const wrapped = await wrapKeyForRecipient(channelKey, recipientPublicKey);
            await chatClient.setChannelKey(token, channelId, login, wrapped);
          }),
      );
      return skippedLogins;
    },
    [chatClient, userClient, getAccessToken, currentLogin],
  );

  return { setUpEncryptedChannel };
}
