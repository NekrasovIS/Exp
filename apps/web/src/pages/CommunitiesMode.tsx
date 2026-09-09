// Issue #266/#267 — the communities/channels/chat navigation, split
// out of HomePage.tsx when issue #268 added a second top-level mode
// (friends/DMs) alongside it.

import { useCallback, useState } from "react";

import { ChatView } from "../chat/ChatView.js";
import { ChannelsSidebar } from "../channels/ChannelsSidebar.js";
import { useChannels } from "../channels/useChannels.js";
import { CommunitiesSidebar } from "../communities/CommunitiesSidebar.js";
import { MembersSidebar } from "../members/MembersSidebar.js";
import styles from "./pageLayout.module.css";

export function CommunitiesMode() {
  const [selectedCommunityId, setSelectedCommunityId] = useState<number | null>(null);
  const [selectedChannelId, setSelectedChannelId] = useState<number | null>(null);
  // Issue #322 — owned here, not by MembersSidebar itself: it's a
  // sibling of ChatView, not a descendant, but presence only arrives
  // over whichever channel socket ChatView happens to have open.
  const [onlineLogins, setOnlineLogins] = useState<ReadonlySet<string>>(new Set());

  // ChannelsSidebar loads its own copy of this same list to render
  // itself — this second call (deduped by nothing, deliberately kept
  // simple) is only to read the selected channel's isEncrypted flag,
  // which ChatView needs and ChannelsSidebar's onSelectChannel(id)
  // contract (already shipped in #266) doesn't carry.
  const { channels } = useChannels(selectedCommunityId);
  const selectedChannel = channels.find((channel) => channel.id === selectedChannelId) ?? null;

  const handleOnlineMembers = useCallback((logins: string[]) => {
    setOnlineLogins(new Set(logins));
  }, []);
  const handlePresenceChanged = useCallback((login: string, online: boolean) => {
    setOnlineLogins((prev) => {
      const next = new Set(prev);
      if (online) {
        next.add(login);
      } else {
        next.delete(login);
      }
      return next;
    });
  }, []);

  function handleSelectCommunity(communityId: number): void {
    setSelectedCommunityId(communityId);
    setSelectedChannelId(null);
    // Presence for the previous community doesn't apply here — same
    // reset DeviceHub's MainWindow does on community switch.
    setOnlineLogins(new Set());
  }

  return (
    <div className={styles.row}>
      <div className={styles.sidebarColumn}>
        <CommunitiesSidebar
          selectedCommunityId={selectedCommunityId}
          onSelectCommunity={handleSelectCommunity}
        />
      </div>
      <div className={styles.sidebarColumn}>
        <ChannelsSidebar
          communityId={selectedCommunityId}
          selectedChannelId={selectedChannelId}
          onSelectChannel={setSelectedChannelId}
        />
      </div>
      <main className={styles.mainColumn}>
        {selectedChannel === null || selectedCommunityId === null ? (
          <p className={styles.placeholder}>Select a channel to start chatting.</p>
        ) : (
          <ChatView
            channelId={selectedChannel.id}
            communityId={selectedCommunityId}
            isEncrypted={selectedChannel.isEncrypted}
            onOnlineMembers={handleOnlineMembers}
            onPresenceChanged={handlePresenceChanged}
          />
        )}
      </main>
      <div className={styles.sidebarColumn}>
        <MembersSidebar communityId={selectedCommunityId} onlineLogins={onlineLogins} />
      </div>
    </div>
  );
}
