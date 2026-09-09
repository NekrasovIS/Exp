// Issue #268 — existing conversations, so one can be reopened directly
// rather than only via FriendsPanel's "Message" button each time.

import { useDmThreads } from "./useDmThreads.js";
import styles from "./DmThreadsList.module.css";

interface DmThreadsListProps {
  selectedThreadId: number | null;
  onSelectThread: (threadId: number, otherLogin: string) => void;
}

export function DmThreadsList({ selectedThreadId, onSelectThread }: DmThreadsListProps) {
  const { threads, loading, error } = useDmThreads();

  return (
    <nav aria-label="Conversations" className={styles.nav}>
      {loading && <p className={styles.mutedText}>Loading conversations…</p>}
      {error !== null && <p role="alert">{error}</p>}
      <ul className={styles.list}>
        {threads.map((thread) => (
          <li key={thread.id}>
            <button
              type="button"
              className={styles.listItemButton}
              aria-current={thread.id === selectedThreadId}
              onClick={() => onSelectThread(thread.id, thread.otherLogin)}
            >
              {thread.otherLogin}
            </button>
          </li>
        ))}
      </ul>
    </nav>
  );
}
