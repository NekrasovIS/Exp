// Issue #267 — composes the message list, composer, and search toggle
// for one (non-encrypted) channel. Split out of ChatView.tsx so that
// component's encrypted-channel early return never mounts this (and so
// never calls useMessages/useChatSocket) for one.

import { useEffect, useState } from "react";

import { CallPanel } from "../calls/CallPanel.js";
import styles from "./chatView.module.css";
import { useIsModerator } from "../communities/useIsModerator.js";
import { useSession } from "../session/SessionContext.js";
import { MessageComposer } from "./MessageComposer.js";
import { MessageList } from "./MessageList.js";
import { MessageSearch } from "./MessageSearch.js";
import { PinnedMessagesPanel } from "./PinnedMessagesPanel.js";
import { useMessages } from "./useMessages.js";
import { usePinnedMessages } from "./usePinnedMessages.js";

interface ChatViewContentProps {
  channelId: number;
  communityId: number;
  // `| undefined` explicit, not just `?:` — ChatView forwards its own
  // already-possibly-undefined prop value verbatim, which
  // exactOptionalPropertyTypes treats differently from the prop being
  // omitted outright.
  onOnlineMembers?: ((logins: string[]) => void) | undefined;
  onPresenceChanged?: ((login: string, online: boolean) => void) | undefined;
}

export function ChatViewContent({
  channelId,
  communityId,
  onOnlineMembers,
  onPresenceChanged,
}: ChatViewContentProps) {
  const { currentLogin } = useSession();
  const isModerator = useIsModerator(communityId);
  const {
    messages,
    editedIds,
    loading,
    error,
    hasMore,
    loadOlder,
    sendMessage,
    editMessage,
    deleteMessage,
    toggleReaction,
    socket,
    typingUser,
    sendTyping,
  } = useMessages(channelId);
  const { pinned, pinnedIds, pin, unpin } = usePinnedMessages(channelId, socket);
  const [searchOpen, setSearchOpen] = useState(false);
  const [pinnedOpen, setPinnedOpen] = useState(false);

  // Issue #322 — presence rides this channel's socket; just forwarded
  // up to the caller, which owns the aggregated state (MembersSidebar
  // is a sibling, not a descendant, of this component).
  useEffect(() => {
    const offOnline = onOnlineMembers !== undefined ? socket.on("onlineMembers", onOnlineMembers) : undefined;
    const offPresence =
      onPresenceChanged !== undefined ? socket.on("presenceChanged", onPresenceChanged) : undefined;
    return () => {
      offOnline?.();
      offPresence?.();
    };
  }, [socket, onOnlineMembers, onPresenceChanged]);

  return (
    <section className={styles.section}>
      {currentLogin !== null && <CallPanel chatClient={socket} localLogin={currentLogin} />}
      <div className={styles.header}>
        <button type="button" onClick={() => setSearchOpen((open) => !open)}>
          {searchOpen ? "Close search" : "Search"}
        </button>
        {pinned.length > 0 && (
          <button type="button" onClick={() => setPinnedOpen((open) => !open)}>
            📌 {pinned.length}
          </button>
        )}
      </div>
      {searchOpen && <MessageSearch channelId={channelId} />}
      {pinnedOpen && <PinnedMessagesPanel pinned={pinned} />}

      <div className={styles.scrollArea}>
        {loading && <p className={styles.statusText}>Loading messages…</p>}
        {error !== null && <p role="alert">{error}</p>}
        {hasMore && !loading && (
          <button type="button" className={styles.loadOlderButton} onClick={() => void loadOlder()}>
            Load older messages
          </button>
        )}
        <MessageList
          messages={messages}
          editedIds={editedIds}
          pinnedIds={pinnedIds}
          currentLogin={currentLogin}
          isModerator={isModerator}
          onEdit={editMessage}
          onDelete={deleteMessage}
          onToggleReaction={toggleReaction}
          onPin={pin}
          onUnpin={unpin}
        />
      </div>
      {typingUser !== null && <p className={styles.statusText}>{typingUser} is typing…</p>}
      <MessageComposer channelId={channelId} onSend={sendMessage} onTyping={sendTyping} />
    </section>
  );
}
