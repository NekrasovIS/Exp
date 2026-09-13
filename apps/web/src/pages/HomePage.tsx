// Issue #266/#267/#268 — top-level mode toggle, mirroring DeviceHub's
// "Friends" button that swaps the sidebar/main-area content between
// communities/channels/chat and friends/DMs.

import { useEffect, useState } from "react";
import { Link } from "react-router-dom";

import styles from "./HomePage.module.css";
import { CommunitiesMode } from "./CommunitiesMode.js";
import { FriendsMode } from "./FriendsMode.js";
import { requestNotificationPermission } from "../notifications/browserNotifications.js";

type Mode = "communities" | "friends";

export function HomePage() {
  const [mode, setMode] = useState<Mode>("communities");

  // Issue #311 — same "ask once, near the top-level authenticated
  // screen" placement as DeviceHub constructing DesktopNotifier from
  // MainWindow's own constructor.
  useEffect(() => {
    requestNotificationPermission();
  }, []);

  return (
    <div className={styles.page}>
      <header className={styles.header}>
        <h1 className={styles.title}>DeviceHub</h1>
        <div className={styles.headerActions}>
          <button type="button" onClick={() => setMode(mode === "communities" ? "friends" : "communities")}>
            {mode === "communities" ? "Friends" : "Back to communities"}
          </button>
          <Link to="/profile">Profile</Link>
        </div>
      </header>
      <div className={styles.body}>{mode === "communities" ? <CommunitiesMode /> : <FriendsMode />}</div>
    </div>
  );
}
