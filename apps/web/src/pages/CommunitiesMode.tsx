// Issue #266/#267 — the communities/channels/chat navigation, split
// out of HomePage.tsx when issue #268 added a second top-level mode
// (friends/DMs) alongside it.

import type { ChatItem } from "@devicehub/core";
import { useState } from "react";

import { ChatView } from "../chat/ChatView.js";
import { ChannelsSidebar } from "../channels/ChannelsSidebar.js";
import { CommunitiesSidebar } from "../communities/CommunitiesSidebar.js";
import styles from "./pageLayout.module.css";

export function CommunitiesMode() {
  const [selectedCommunityId, setSelectedCommunityId] = useState<number | null>(null);
  const [selectedChannel, setSelectedChannel] = useState<ChatItem | null>(null);

  function handleSelectCommunity(communityId: number): void {
    setSelectedCommunityId(communityId);
    setSelectedChannel(null);
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
          selectedChannelId={selectedChannel?.id ?? null}
          onSelectChannel={setSelectedChannel}
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
