// Issue #267 — composes the message list, composer, and search toggle
// for one (non-encrypted) channel. Split out of ChatView.tsx so that
// component's encrypted-channel early return never mounts this (and so
// never calls useMessages/useChatSocket) for one.

import { useState } from "react";

import { CallPanel } from "../calls/CallPanel.js";
import styles from "./chatView.module.css";
import { useIsModerator } from "../communities/useIsModerator.js";
import { useSession } from "../session/SessionContext.js";
import { MessageComposer } from "./MessageComposer.js";
import { MessageList } from "./MessageList.js";
import { MessageSearch } from "./MessageSearch.js";
import { useMessages } from "./useMessages.js";

interface ChatViewContentProps {
  channelId: number;
  communityId: number;
}

export function ChatViewContent({ channelId, communityId }: ChatViewContentProps) {
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
    socket,
    typingUser,
    sendTyping,
  } = useMessages(channelId);
  const [searchOpen, setSearchOpen] = useState(false);

  return (
    <section className={styles.section}>
      {currentLogin !== null && <CallPanel chatClient={socket} localLogin={currentLogin} />}
      <div className={styles.header}>
        <button type="button" onClick={() => setSearchOpen((open) => !open)}>
          {searchOpen ? "Close search" : "Search"}
        </button>
      </div>
      {searchOpen && <MessageSearch channelId={channelId} />}

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
          currentLogin={currentLogin}
          isModerator={isModerator}
          onEdit={editMessage}
          onDelete={deleteMessage}
        />
      </div>
      {typingUser !== null && <p className={styles.statusText}>{typingUser} is typing…</p>}
      <MessageComposer channelId={channelId} onSend={sendMessage} onTyping={sendTyping} />
    </section>
  );
}
