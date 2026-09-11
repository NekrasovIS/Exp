// Issue #268 — existing conversations, so one can be reopened directly
// rather than only via FriendsPanel's "Message" button each time.

import { useDmThreads } from "./useDmThreads.js";
import styles from "./DmThreadsList.module.css";

interface DmThreadsListProps {
  selectedThreadId: number | null;
  onSelectThread: (threadId: number, otherLogin: string) => void;
  /** Unread message count per thread id (issue #310/#350). Unlike
   * DeviceHub (which has no persistent DM thread list to badge — see
   * its own README note), this list can show one directly, since it
   * already carries the same thread ids fetchUnreadCounts() reports. */
  unreadCounts?: ReadonlyMap<number, number>;
}

export function DmThreadsList({ selectedThreadId, onSelectThread, unreadCounts }: DmThreadsListProps) {
  const { threads, loading, error } = useDmThreads();

  return (
    <nav aria-label="Conversations" className={styles.nav}>
      {loading && <p className={styles.mutedText}>Loading conversations…</p>}
      {error !== null && <p role="alert">{error}</p>}
      <ul className={styles.list}>
        {threads.map((thread) => {
          const unreadCount = unreadCounts?.get(thread.id) ?? 0;
          return (
            <li key={thread.id}>
              <button
                type="button"
                className={styles.listItemButton}
                aria-current={thread.id === selectedThreadId}
                onClick={() => onSelectThread(thread.id, thread.otherLogin)}
              >
                <span className={unreadCount > 0 ? styles.unreadLabel : undefined}>{thread.otherLogin}</span>
                {unreadCount > 0 && (
                  <span className={styles.unreadBadge}>{unreadCount > 99 ? "99+" : unreadCount}</span>
                )}
              </button>
            </li>
          );
        })}
      </ul>
    </nav>
  );
}
