// Issue #266/#267/#268 — top-level mode toggle, mirroring DeviceHub's
// "Friends" button that swaps the sidebar/main-area content between
// communities/channels/chat and friends/DMs.

import { useState } from "react";

import { CommunitiesMode } from "./CommunitiesMode.js";
import { FriendsMode } from "./FriendsMode.js";

type Mode = "communities" | "friends";

export function HomePage() {
  const [mode, setMode] = useState<Mode>("communities");

  return (
    <div>
      <button type="button" onClick={() => setMode(mode === "communities" ? "friends" : "communities")}>
        {mode === "communities" ? "Friends" : "Back to communities"}
      </button>
      {mode === "communities" ? <CommunitiesMode /> : <FriendsMode />}
    </div>
  );
}
