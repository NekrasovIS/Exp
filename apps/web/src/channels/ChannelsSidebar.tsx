// Issue #266 — analog of DeviceHub's ChannelsPanel: the channel list
// for whichever community is currently selected (CommunitiesSidebar's
// job), selection, and creating a new channel. Renders nothing (not an
// empty-state message) when no community is selected — the caller
// decides what that gap looks like.

import { useState, type FormEvent } from "react";

import { useChannels } from "./useChannels.js";

interface ChannelsSidebarProps {
  communityId: number | null;
  selectedChannelId: number | null;
  onSelectChannel: (channelId: number) => void;
}

export function ChannelsSidebar({ communityId, selectedChannelId, onSelectChannel }: ChannelsSidebarProps) {
  const { channels, loading, error, createChannel } = useChannels(communityId);
  const [name, setName] = useState("");
  const [creating, setCreating] = useState(false);
  const [createError, setCreateError] = useState<string | null>(null);

  if (communityId === null) {
    return null;
  }

  async function handleCreate(event: FormEvent): Promise<void> {
    event.preventDefault();
    if (name.trim() === "") {
      return;
    }
    setCreateError(null);
    setCreating(true);
    try {
      await createChannel(name.trim());
      setName("");
    } catch {
      setCreateError("Couldn't create that channel — the name may already be taken.");
    } finally {
      setCreating(false);
    }
  }

  return (
    <nav aria-label="Channels">
      {loading && <p>Loading channels…</p>}
      {error !== null && <p role="alert">{error}</p>}
      <ul>
        {channels.map((channel) => (
          <li key={channel.id}>
            <button
              type="button"
              aria-current={channel.id === selectedChannelId}
              onClick={() => onSelectChannel(channel.id)}
            >
              #{channel.name}
            </button>
          </li>
        ))}
      </ul>
      <form onSubmit={handleCreate}>
        <label htmlFor="new-channel-name">New channel</label>
        <input id="new-channel-name" value={name} onChange={(event) => setName(event.target.value)} />
        {createError !== null && <p role="alert">{createError}</p>}
        <button type="submit" disabled={creating}>
          Create
        </button>
      </form>
    </nav>
  );
}
