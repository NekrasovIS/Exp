// Issue #267/#269 — top-level channel content area. Non-encrypted
// channels render ChatViewContent directly; encrypted ones fetch+unwrap
// this login's copy of the channel key first (useChannelKey) and only
// mount EncryptedChatViewContent once it's available — "no key on file
// yet" and "unwrap failed" both show the same no-access message,
// mirroring how DeviceHub's own MainWindow.cpp treats them.

import { useChannelKey } from "../crypto/useChannelKey.js";
import { useIdentityKeys } from "../crypto/useIdentityKeys.js";
import { ChatViewContent } from "./ChatViewContent.js";
import { EncryptedChatViewContent } from "./EncryptedChatViewContent.js";

interface ChatViewProps {
  channelId: number;
  communityId: number;
  isEncrypted: boolean;
}

export function ChatView({ channelId, communityId, isEncrypted }: ChatViewProps) {
  const identityKeys = useIdentityKeys();
  const { loading, channelKey } = useChannelKey(channelId, isEncrypted ? identityKeys : null);

  if (!isEncrypted) {
    return <ChatViewContent channelId={channelId} communityId={communityId} />;
  }
  if (loading) {
    return <p>Loading…</p>;
  }
  if (channelKey === null) {
    return <p>You don't have access to this encrypted channel yet.</p>;
  }
  return <EncryptedChatViewContent channelId={channelId} communityId={communityId} channelKey={channelKey} />;
}
