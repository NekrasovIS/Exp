// Issue #268 — composes the DM history list, load-older, and composer
// for one thread, mirroring ChatViewContent's shape (#267) but for the
// simpler DM feature set.

import { AsyncListStatus } from "@devicehub/ui";

import styles from "../chat/chatView.module.css";
import { useReadReceipts } from "../chat/useReadReceipts.js";
import { useSession } from "../session/SessionContext.js";
import { DirectMessageComposer } from "./DirectMessageComposer.js";
import { DirectMessageList } from "./DirectMessageList.js";
import { useDirectMessages } from "./useDirectMessages.js";

interface DirectMessageViewProps {
  threadId: number;
  otherLogin: string;
}

export function DirectMessageView({ threadId, otherLogin }: DirectMessageViewProps) {
  const { currentLogin } = useSession();
  const { messages, loading, error, hasMore, loadOlder, sendMessage, socket, typingUser, sendTyping } =
    useDirectMessages(threadId);
  const { readPointers } = useReadReceipts({ dmThreadId: threadId }, socket);

  return (
    <section className={styles.section}>
      <div className={styles.header}>
        <h2>{otherLogin}</h2>
      </div>
      <div className={styles.scrollArea}>
        <AsyncListStatus loading={loading} error={error} loadingText="Loading messages…" />
        {hasMore && !loading && (
          <button type="button" className={styles.loadOlderButton} onClick={() => void loadOlder()}>
            Load older messages
          </button>
        )}
        <DirectMessageList
          messages={messages}
          currentLogin={currentLogin}
          otherLogin={otherLogin}
          readPointers={readPointers}
        />
      </div>
      {typingUser !== null && <p className={styles.statusText}>{typingUser} is typing…</p>}
      <DirectMessageComposer threadId={threadId} onSend={sendMessage} onTyping={sendTyping} />
    </section>
  );
}
