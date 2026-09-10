// Issue #266 — analog of DeviceHub's CommunitiesPanel: the list of
// communities the caller has joined, selection, and joining a new one
// by invite code. Doesn't know about channels or chat content — that's
// ChannelsSidebar/#267's job.
//
// Invite-link block (issue #301): every member can copy the currently
// selected community's invite link (mirrors DeviceHub's
// CommunitiesPanel context menu — "Copy Invite Code" for any member),
// only the owner can regenerate it (same "Regenerate Invite Code"
// restriction, and the same server-side check — regenerateInviteCode()
// 403s for a non-owner member, see ChatRestClient's own doc comment).
// The link itself is just `${origin}/join/${code}` — JoinPage.tsx is
// the other half of this feature.

import { useState, type FormEvent } from "react";

import { useCommunities } from "./useCommunities.js";
import styles from "../pages/sidebarNav.module.css";
import { useSession } from "../session/SessionContext.js";

interface CommunitiesSidebarProps {
  selectedCommunityId: number | null;
  onSelectCommunity: (communityId: number) => void;
  /** Sum of unread messages across a community's channels (issue
   * #310/#350), computed by the caller (CommunitiesMode) from the
   * channel→community map it accumulates as communities get opened —
   * a community never opened this session has no entry here yet, same
   * scope cut as DeviceHub's own channelIdToCommunityId_. */
  unreadCounts?: ReadonlyMap<number, number>;
}

export function CommunitiesSidebar({
  selectedCommunityId,
  onSelectCommunity,
  unreadCounts,
}: CommunitiesSidebarProps) {
  const { currentLogin } = useSession();
  const { communities, loading, error, joinByCode, regenerateInviteCode } = useCommunities();
  const [code, setCode] = useState("");
  const [joinError, setJoinError] = useState<string | null>(null);
  const [joining, setJoining] = useState(false);
  const [copied, setCopied] = useState(false);
  const [regenerating, setRegenerating] = useState(false);

  const selectedCommunity = communities.find((community) => community.id === selectedCommunityId) ?? null;

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

  async function handleCopyInviteLink(): Promise<void> {
    if (selectedCommunity?.inviteCode === undefined) {
      return;
    }
    const link = `${window.location.origin}/join/${selectedCommunity.inviteCode}`;
    await navigator.clipboard.writeText(link);
    setCopied(true);
    setTimeout(() => setCopied(false), 2000);
  }

  async function handleRegenerate(): Promise<void> {
    if (selectedCommunity === null) {
      return;
    }
    setRegenerating(true);
    try {
      await regenerateInviteCode(selectedCommunity.id);
    } finally {
      setRegenerating(false);
    }
  }

  return (
    <nav aria-label="Communities" className={styles.nav}>
      {loading && <p className={styles.mutedText}>Loading communities…</p>}
      {error !== null && <p role="alert">{error}</p>}
      <ul className={styles.list}>
        {communities.map((community) => {
          const unreadCount = unreadCounts?.get(community.id) ?? 0;
          return (
            <li key={community.id}>
              <button
                type="button"
                className={styles.listItemButton}
                aria-current={community.id === selectedCommunityId}
                onClick={() => onSelectCommunity(community.id)}
              >
                <span className={unreadCount > 0 ? styles.unreadLabel : undefined}>{community.name}</span>
                {unreadCount > 0 && (
                  <span className={styles.unreadBadge}>{unreadCount > 99 ? "99+" : unreadCount}</span>
                )}
              </button>
            </li>
          );
        })}
      </ul>
      {selectedCommunity?.inviteCode !== undefined && (
        <div className={styles.inviteBlock}>
          <p>Invite link</p>
          <div className={styles.inviteRow}>
            <input
              className={styles.inviteLinkInput}
              readOnly
              value={`${window.location.origin}/join/${selectedCommunity.inviteCode}`}
              aria-label="Invite link"
            />
            <button type="button" onClick={() => void handleCopyInviteLink()}>
              {copied ? "Copied!" : "Copy"}
            </button>
          </div>
          {selectedCommunity.ownerLogin === currentLogin && (
            <button type="button" onClick={() => void handleRegenerate()} disabled={regenerating}>
              Regenerate
            </button>
          )}
        </div>
      )}
      <form onSubmit={handleJoin} className={styles.form}>
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
