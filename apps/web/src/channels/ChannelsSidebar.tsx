// Issue #266/#269 — analog of DeviceHub's ChannelsPanel: the channel
// list for whichever community is currently selected
// (CommunitiesSidebar's job), selection, and creating a new channel
// (optionally encrypted). Renders nothing (not an empty-state message)
// when no community is selected — the caller decides what that gap
// looks like.

import { useState, type FormEvent } from "react";

import { useChannels } from "./useChannels.js";
import { useEncryptedChannelSetup } from "../crypto/useEncryptedChannelSetup.js";
import { useIdentityKeys } from "../crypto/useIdentityKeys.js";

interface ChannelsSidebarProps {
  communityId: number | null;
  selectedChannelId: number | null;
  onSelectChannel: (channelId: number) => void;
}

export function ChannelsSidebar({ communityId, selectedChannelId, onSelectChannel }: ChannelsSidebarProps) {
  const { channels, loading, error, createChannel } = useChannels(communityId);
  const identityKeys = useIdentityKeys();
  const { setUpEncryptedChannel } = useEncryptedChannelSetup();
  const [name, setName] = useState("");
  const [isEncrypted, setIsEncrypted] = useState(false);
  const [creating, setCreating] = useState(false);
  const [createError, setCreateError] = useState<string | null>(null);
  const [skippedLogins, setSkippedLogins] = useState<string[]>([]);

  if (communityId === null) {
    return null;
  }

  async function handleCreate(event: FormEvent): Promise<void> {
    event.preventDefault();
    if (name.trim() === "" || communityId === null) {
      return;
    }
    setCreateError(null);
    setSkippedLogins([]);
    setCreating(true);
    try {
      const created = await createChannel(name.trim(), isEncrypted);
      if (created !== null && isEncrypted && identityKeys !== null) {
        // The channel itself already exists at this point regardless
        // of what happens next — a failure here means some members
        // won't have access yet, not that creation failed, so it's
        // reported separately from createError.
        setSkippedLogins(await setUpEncryptedChannel(created.id, communityId, identityKeys));
      }
      setName("");
      setIsEncrypted(false);
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
              {channel.isEncrypted ? "🔒 " : ""}#{channel.name}
            </button>
          </li>
        ))}
      </ul>
      <form onSubmit={handleCreate}>
        <label htmlFor="new-channel-name">New channel</label>
        <input id="new-channel-name" value={name} onChange={(event) => setName(event.target.value)} />
        <label htmlFor="new-channel-encrypted">
          <input
            id="new-channel-encrypted"
            type="checkbox"
            checked={isEncrypted}
            onChange={(event) => setIsEncrypted(event.target.checked)}
          />
          Encrypted channel
        </label>
        {createError !== null && <p role="alert">{createError}</p>}
        {skippedLogins.length > 0 && (
          <p role="alert">
            {skippedLogins.join(", ")} haven't set up encryption yet and won't have access to this channel.
          </p>
        )}
        <button type="submit" disabled={creating}>
          Create
        </button>
      </form>
    </nav>
  );
}
