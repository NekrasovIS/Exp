// Issue #266 — composes the communities/channels navigation. The chat
// content area for the selected channel (#267) and the friends/DM mode
// (#268) aren't built yet, so this still ends in a placeholder for
// those, just one that now reflects a real selected channel id.

import { useState } from "react";

import { ChannelsSidebar } from "../channels/ChannelsSidebar.js";
import { CommunitiesSidebar } from "../communities/CommunitiesSidebar.js";

export function HomePage() {
  const [selectedCommunityId, setSelectedCommunityId] = useState<number | null>(null);
  const [selectedChannelId, setSelectedChannelId] = useState<number | null>(null);

  function handleSelectCommunity(communityId: number): void {
    setSelectedCommunityId(communityId);
    setSelectedChannelId(null);
  }

  return (
    <div>
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
        {selectedChannelId === null ? (
          <p>Select a channel to start chatting.</p>
        ) : (
          <p>Channel view for #{selectedChannelId} lands in issue #267.</p>
        )}
      </main>
    </div>
  );
}
