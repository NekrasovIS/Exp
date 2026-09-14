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
import { useIsNarrowViewport } from "./useIsNarrowViewport.js";

type NarrowTab = "friends" | "threads";

export function FriendsMode() {
  const isNarrow = useIsNarrowViewport();
  const { openThreadWith } = useDmThreads();
  const [selectedThread, setSelectedThread] = useState<{ id: number; otherLogin: string } | null>(null);
  // Issue #439 — which of the two list panels shows on a narrow
  // viewport while no thread is open; irrelevant (and unused) on a wide
  // viewport, where both always show side by side.
  const [narrowTab, setNarrowTab] = useState<NarrowTab>("friends");
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

  // Issue #439 — same "one panel at a time" idea as CommunitiesMode, but
  // Friends/DM-threads aren't a hierarchy (neither depends on the
  // other being selected first), so a tab switch between them replaces
  // the community/channel drill-down step. On a wide viewport every
  // condition below is true, unchanged from before.
  const showFriendsPanel = !isNarrow || (selectedThread === null && narrowTab === "friends");
  const showDmThreadsList = !isNarrow || (selectedThread === null && narrowTab === "threads");
  const showMain = !isNarrow || selectedThread !== null;
  const showNarrowTabs = isNarrow && selectedThread === null;

  return (
    <div className={styles.stack}>
      {showNarrowTabs && (
        <div className={styles.narrowTabs}>
          <button
            type="button"
            className={styles.narrowTab}
            aria-pressed={narrowTab === "friends"}
            onClick={() => setNarrowTab("friends")}
          >
            Friends
          </button>
          <button
            type="button"
            className={styles.narrowTab}
            aria-pressed={narrowTab === "threads"}
            onClick={() => setNarrowTab("threads")}
          >
            Messages
          </button>
        </div>
      )}
      <div className={styles.row}>
        {showFriendsPanel && (
          <div className={styles.sidebarColumn}>
            <FriendsPanel onOpenThreadWith={(login) => void handleOpenThreadWith(login)} />
          </div>
        )}
        {showDmThreadsList && (
          <div className={styles.sidebarColumn}>
            <DmThreadsList
              selectedThreadId={selectedThread?.id ?? null}
              onSelectThread={handleSelectThread}
              unreadCounts={threadCounts}
            />
          </div>
        )}
        {showMain && (
          <main className={styles.mainColumn}>
            {isNarrow && (
              <button type="button" className={styles.backButton} onClick={() => setSelectedThread(null)}>
                ← Messages
              </button>
            )}
            {selectedThread === null ? (
              <p className={styles.placeholder}>Select a conversation to start chatting.</p>
            ) : (
              <DirectMessageView threadId={selectedThread.id} otherLogin={selectedThread.otherLogin} />
            )}
          </main>
        )}
      </div>
    </div>
  );
}
