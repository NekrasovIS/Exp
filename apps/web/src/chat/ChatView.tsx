// Issue #267/#269 — top-level channel content area. Non-encrypted
// channels render ChatViewContent directly; encrypted ones fetch+unwrap
// this login's copy of the channel key first (useChannelKey) and only
// mount EncryptedChatViewContent once it's available — "no key on file
// yet" and "unwrap failed" both show the same no-access message,
// mirroring how DeviceHub's own MainWindow.cpp treats them.

import { useChannelKey } from "../crypto/useChannelKey.js";
import { useIdentityKeys } from "../crypto/useIdentityKeys.js";
import placeholderStyles from "../pages/pageLayout.module.css";
import { ChatViewContent } from "./ChatViewContent.js";
import { EncryptedChatViewContent } from "./EncryptedChatViewContent.js";

interface ChatViewProps {
  channelId: number;
  communityId: number;
  isEncrypted: boolean;
  /** Presence (issue #322) — forwarded from whichever channel socket is
   * actually subscribed right now (ChatViewContent's or
   * EncryptedChatViewContent's own useMessages()); the caller
   * (CommunitiesMode) owns the aggregated online-logins state since
   * MembersSidebar is a sibling of this component, not a descendant. */
  onOnlineMembers?: (logins: string[]) => void;
  onPresenceChanged?: (login: string, online: boolean) => void;
}

export function ChatView({
  channelId,
  communityId,
  isEncrypted,
  onOnlineMembers,
  onPresenceChanged,
}: ChatViewProps) {
  const identityKeys = useIdentityKeys();
  const { loading, channelKey } = useChannelKey(channelId, isEncrypted ? identityKeys : null);

  if (!isEncrypted) {
    return (
      <ChatViewContent
        channelId={channelId}
        communityId={communityId}
        onOnlineMembers={onOnlineMembers}
        onPresenceChanged={onPresenceChanged}
      />
    );
  }
  if (loading) {
    return <p className={placeholderStyles.placeholder}>Loading…</p>;
  }
  if (channelKey === null) {
    return (
      <p className={placeholderStyles.placeholder}>You don't have access to this encrypted channel yet.</p>
    );
  }
  return (
    <EncryptedChatViewContent
      channelId={channelId}
      communityId={communityId}
      channelKey={channelKey}
      onOnlineMembers={onOnlineMembers}
      onPresenceChanged={onPresenceChanged}
    />
  );
}
