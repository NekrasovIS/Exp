// Issue #268 — friends/DM navigation, the second top-level mode
// alongside CommunitiesMode (toggled from HomePage). Opening a thread
// from FriendsPanel's "Message" button uses this component's own
// useDmThreads() instance for the actual open-by-login call, rather
// than threading a callback down into DmThreadsList — that component
// only needs to list/select already-open threads.

import { useState } from "react";

import { useUnreadCounts } from "../chat/useUnreadCounts.js";
import { DirectMessageView } from "../dm/DirectMessageView.js";
import { DmThreadsList } from "../dm/DmThreadsList.js";
import { useDmThreads } from "../dm/useDmThreads.js";
import { FriendsPanel } from "../friends/FriendsPanel.js";
import styles from "./pageLayout.module.css";

export function FriendsMode() {
  const { openThreadWith } = useDmThreads();
  const [selectedThread, setSelectedThread] = useState<{ id: number; otherLogin: string } | null>(null);
  const { threadCounts, clearThreadLocally } = useUnreadCounts();

  async function handleOpenThreadWith(login: string): Promise<void> {
    const id = await openThreadWith(login);
    if (id !== null) {
      setSelectedThread({ id, otherLogin: login });
      clearThreadLocally(id);
    }
  }

  function handleSelectThread(id: number, otherLogin: string): void {
    setSelectedThread({ id, otherLogin });
    clearThreadLocally(id);
  }

  return (
    <div className={styles.row}>
      <div className={styles.sidebarColumn}>
        <FriendsPanel onOpenThreadWith={(login) => void handleOpenThreadWith(login)} />
      </div>
      <div className={styles.sidebarColumn}>
        <DmThreadsList
          selectedThreadId={selectedThread?.id ?? null}
          onSelectThread={handleSelectThread}
          unreadCounts={threadCounts}
        />
      </div>
      <main className={styles.mainColumn}>
        {selectedThread === null ? (
          <p className={styles.placeholder}>Select a conversation to start chatting.</p>
        ) : (
          <DirectMessageView threadId={selectedThread.id} otherLogin={selectedThread.otherLogin} />
        )}
      </main>
    </div>
  );
}
