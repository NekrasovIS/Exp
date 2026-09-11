// Issue #266/#267 — the communities/channels/chat navigation, split
// out of HomePage.tsx when issue #268 added a second top-level mode
// (friends/DMs) alongside it.

import type { ChatItem } from "@devicehub/core";
import { useCallback, useEffect, useMemo, useState } from "react";

import { ChatView } from "../chat/ChatView.js";
import { useUnreadCounts } from "../chat/useUnreadCounts.js";
import { ChannelsSidebar } from "../channels/ChannelsSidebar.js";
import { useChannels } from "../channels/useChannels.js";
import { CommunitiesSidebar } from "../communities/CommunitiesSidebar.js";
import { MembersSidebar } from "../members/MembersSidebar.js";
import styles from "./pageLayout.module.css";

export function CommunitiesMode() {
  const [selectedCommunityId, setSelectedCommunityId] = useState<number | null>(null);
  const [selectedChannel, setSelectedChannel] = useState<ChatItem | null>(null);
  // Issue #322 — owned here, not by MembersSidebar itself: it's a
  // sibling of ChatView, not a descendant, but presence only arrives
  // over whichever channel socket ChatView happens to have open.
  const [onlineLogins, setOnlineLogins] = useState<ReadonlySet<string>>(new Set());

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

  // Issue #310/#350 — only used to build channelIdToCommunityId below
  // (a channel→community lookup for summing badges per community);
  // selectedChannel itself comes straight from ChannelsSidebar's
  // onSelectChannel(channel) below, not derived from this list, so it
  // never goes stale the way issue #303 found (a second copy of the
  // channels list that missed a just-created channel).
  const { channels } = useChannels(selectedCommunityId);

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
    setSelectedChannel(null);
    // Own badges may have drifted while looking at a different
    // community — don't wait for the next poll tick (issue #310/#350).
    void refreshUnreadCounts();
    // Presence for the previous community doesn't apply here — same
    // reset DeviceHub's MainWindow does on community switch.
    setOnlineLogins(new Set());
  }

  function handleSelectChannel(channel: ChatItem): void {
    setSelectedChannel(channel);
    clearChannelLocally(channel.id);
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
          selectedChannelId={selectedChannel?.id ?? null}
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
