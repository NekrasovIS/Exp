// Issue #322 — community member list with presence, mirroring
// DeviceHub's MemberListPanel (issue #182 for the list itself, #309 for
// the presence dot). Renders nothing when no community is selected,
// same convention ChannelsSidebar/CommunitiesSidebar use.
//
// Issue #385 — the avatar circle renders a real uploaded image (Avatar)
// instead of always the letter placeholder.

import { Avatar } from "../profile/Avatar.js";
import styles from "./membersSidebar.module.css";
import { useMembers } from "./useMembers.js";

interface MembersSidebarProps {
  communityId: number | null;
  /** Logins with at least one live connection to this community right
   * now — owned by the caller (CommunitiesMode), which is what actually
   * receives ChatView's onOnlineMembers/onPresenceChanged callbacks;
   * this component only renders the dot. */
  onlineLogins: ReadonlySet<string>;
}

export function MembersSidebar({ communityId, onlineLogins }: MembersSidebarProps) {
  const { members, loading, error } = useMembers(communityId);

  if (communityId === null) {
    return null;
  }

  return (
    <nav aria-label="Members" className={styles.nav}>
      <p className={styles.title}>MEMBERS — {members.length}</p>
      {loading && <p className={styles.mutedText}>Loading members…</p>}
      {error !== null && <p role="alert">{error}</p>}
      <ul className={styles.list}>
        {members.map((login) => (
          <li key={login} className={styles.row}>
            <span className={styles.avatar}>
              <Avatar login={login} />
              {onlineLogins.has(login) && <span className={styles.onlineDot} title="Online" />}
            </span>
            <span className={styles.login}>{login}</span>
          </li>
        ))}
      </ul>
    </nav>
  );
}
