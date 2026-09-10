// Issue #266/#267 — the communities/channels/chat navigation, split
// out of HomePage.tsx when issue #268 added a second top-level mode
// (friends/DMs) alongside it.

import { useEffect, useMemo, useState } from "react";

import { ChatView } from "../chat/ChatView.js";
import { useUnreadCounts } from "../chat/useUnreadCounts.js";
import { ChannelsSidebar } from "../channels/ChannelsSidebar.js";
import { useChannels } from "../channels/useChannels.js";
import { CommunitiesSidebar } from "../communities/CommunitiesSidebar.js";
import styles from "./pageLayout.module.css";

export function CommunitiesMode() {
  const [selectedCommunityId, setSelectedCommunityId] = useState<number | null>(null);
  const [selectedChannelId, setSelectedChannelId] = useState<number | null>(null);

  // ChannelsSidebar loads its own copy of this same list to render
  // itself — this second call (deduped by nothing, deliberately kept
  // simple) is only to read the selected channel's isEncrypted flag,
  // which ChatView needs and ChannelsSidebar's onSelectChannel(id)
  // contract (already shipped in #266) doesn't carry.
  const { channels } = useChannels(selectedCommunityId);
  const selectedChannel = channels.find((channel) => channel.id === selectedChannelId) ?? null;

  const { channelCounts, refresh: refreshUnreadCounts, clearChannelLocally } = useUnreadCounts();

  // Issue #310/#350 — accumulates channel→community across whichever
  // communities have actually been opened this session (fetchUnreadCounts()
  // returns bare channel ids with no community of their own), same scope
  // cut as DeviceHub's channelIdToCommunityId_: a community never opened
  // this session shows no badge until it is.
  const [channelIdToCommunityId, setChannelIdToCommunityId] = useState<ReadonlyMap<number, number>>(
    new Map(),
  );
  useEffect(() => {
    if (selectedCommunityId === null || channels.length === 0) {
      return;
    }
    setChannelIdToCommunityId((prev) => {
      const next = new Map(prev);
      for (const channel of channels) {
        next.set(channel.id, selectedCommunityId);
      }
      return next;
    });
  }, [channels, selectedCommunityId]);

  const communityCounts = useMemo(() => {
    const sums = new Map<number, number>();
    for (const [channelId, count] of channelCounts) {
      const communityId = channelIdToCommunityId.get(channelId);
      if (communityId !== undefined) {
        sums.set(communityId, (sums.get(communityId) ?? 0) + count);
      }
    }
    return sums;
  }, [channelCounts, channelIdToCommunityId]);

  function handleSelectCommunity(communityId: number): void {
    setSelectedCommunityId(communityId);
    setSelectedChannelId(null);
    // Own badges may have drifted while looking at a different
    // community — don't wait for the next poll tick (issue #310/#350).
    void refreshUnreadCounts();
  }

  function handleSelectChannel(channelId: number): void {
    setSelectedChannelId(channelId);
    clearChannelLocally(channelId);
  }

  return (
    <div className={styles.row}>
      <div className={styles.sidebarColumn}>
        <CommunitiesSidebar
          selectedCommunityId={selectedCommunityId}
          onSelectCommunity={handleSelectCommunity}
          unreadCounts={communityCounts}
        />
      </div>
      <div className={styles.sidebarColumn}>
        <ChannelsSidebar
          communityId={selectedCommunityId}
          selectedChannelId={selectedChannelId}
          onSelectChannel={handleSelectChannel}
          unreadCounts={channelCounts}
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
          />
        )}
      </main>
    </div>
  );
}
