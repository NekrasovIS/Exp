// Issue #267 — composes the message list, composer, and search toggle
// for one (non-encrypted) channel. Split out of ChatView.tsx so that
// component's encrypted-channel early return never mounts this (and so
// never calls useMessages/useChatSocket) for one.

import type { ChatMessageInfo } from "@devicehub/core";
import { useState } from "react";

import { CallPanel } from "../calls/CallPanel.js";
import styles from "./chatView.module.css";
import { useIsModerator } from "../communities/useIsModerator.js";
import { useSession } from "../session/SessionContext.js";
import { MessageComposer } from "./MessageComposer.js";
import { MessageList, truncatedSnippet } from "./MessageList.js";
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
  } = useMessages(channelId);
  const [searchOpen, setSearchOpen] = useState(false);
  // Reply target (issue #306/#331) — resolved from `messages` itself,
  // same client-side model as DeviceHub's ChatView; cleared once the
  // composer actually sends (see MessageComposer's onCancelReply).
  const [replyTarget, setReplyTarget] = useState<ChatMessageInfo | null>(null);

  function handleSend(body: string, attachmentId?: number): void {
    sendMessage(body, attachmentId, replyTarget?.id);
  }

  function handleReply(id: number): void {
    const target = messages.find((m) => m.id === id);
    if (target !== undefined) {
      setReplyTarget(target);
    }
  }

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
          onReply={handleReply}
        />
      </div>
      <MessageComposer
        channelId={channelId}
        onSend={handleSend}
        replyTarget={
          replyTarget !== null
            ? { id: replyTarget.id, author: replyTarget.author, snippet: truncatedSnippet(replyTarget.body) }
            : null
        }
        onCancelReply={() => setReplyTarget(null)}
      />
    </section>
  );
}
