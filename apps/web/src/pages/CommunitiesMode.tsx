// Issue #266/#267 — the communities/channels/chat navigation, split
// out of HomePage.tsx when issue #268 added a second top-level mode
// (friends/DMs) alongside it.

import { useState } from "react";

import { ChatView } from "../chat/ChatView.js";
import { ChannelsSidebar } from "../channels/ChannelsSidebar.js";
import { useChannels } from "../channels/useChannels.js";
import { CommunitiesSidebar } from "../communities/CommunitiesSidebar.js";

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

  function handleSelectCommunity(communityId: number): void {
    setSelectedCommunityId(communityId);
    setSelectedChannelId(null);
  }

  return (
    <>
      <CommunitiesSidebar
        selectedCommunityId={selectedCommunityId}
        onSelectCommunity={handleSelectCommunity}
      />
      <ChannelsSidebar
        communityId={selectedCommunityId}
        selectedChannelId={selectedChannelId}
        onSelectChannel={setSelectedChannelId}
      />
      <main>
        {selectedChannel === null || selectedCommunityId === null ? (
          <p>Select a channel to start chatting.</p>
        ) : (
          <ChatView
            channelId={selectedChannel.id}
            communityId={selectedCommunityId}
            isEncrypted={selectedChannel.isEncrypted}
          />
        )}
      </main>
    </>
  );
}
