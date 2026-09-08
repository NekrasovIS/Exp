// Issue #266 — analog of DeviceHub's CommunitiesPanel: the list of
// communities the caller has joined, selection, and joining a new one
// by invite code. Doesn't know about channels or chat content — that's
// ChannelsSidebar/#267's job.

import { useState, type FormEvent } from "react";

import { useCommunities } from "./useCommunities.js";

interface CommunitiesSidebarProps {
  selectedCommunityId: number | null;
  onSelectCommunity: (communityId: number) => void;
}

export function CommunitiesSidebar({ selectedCommunityId, onSelectCommunity }: CommunitiesSidebarProps) {
  const { communities, loading, error, joinByCode } = useCommunities();
  const [code, setCode] = useState("");
  const [joinError, setJoinError] = useState<string | null>(null);
  const [joining, setJoining] = useState(false);

  async function handleJoin(event: FormEvent): Promise<void> {
    event.preventDefault();
    if (code.trim() === "") {
      return;
    }
    setJoinError(null);
    setJoining(true);
    try {
      await joinByCode(code.trim());
      setCode("");
    } catch {
      setJoinError("That invite code doesn't match any community.");
    } finally {
      setJoining(false);
    }
  }

  return (
    <nav aria-label="Communities">
      {loading && <p>Loading communities…</p>}
      {error !== null && <p role="alert">{error}</p>}
      <ul>
        {communities.map((community) => (
          <li key={community.id}>
            <button
              type="button"
              aria-current={community.id === selectedCommunityId}
              onClick={() => onSelectCommunity(community.id)}
            >
              {community.name}
            </button>
          </li>
        ))}
      </ul>
      <form onSubmit={handleJoin}>
        <label htmlFor="community-invite-code">Invite code</label>
        <input id="community-invite-code" value={code} onChange={(event) => setCode(event.target.value)} />
        {joinError !== null && <p role="alert">{joinError}</p>}
        <button type="submit" disabled={joining}>
          Join
        </button>
      </form>
    </nav>
  );
}
