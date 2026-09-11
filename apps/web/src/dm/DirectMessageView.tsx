// Issue #268 — composes the DM history list, load-older, and composer
// for one thread, mirroring ChatViewContent's shape (#267) but for the
// simpler DM feature set.

import styles from "../chat/chatView.module.css";
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
  const { messages, loading, error, hasMore, loadOlder, sendMessage, typingUser, sendTyping } =
    useDirectMessages(threadId);

  return (
    <section className={styles.section}>
      <div className={styles.header}>
        <h2>{otherLogin}</h2>
      </div>
      <div className={styles.scrollArea}>
        {loading && <p className={styles.statusText}>Loading messages…</p>}
        {error !== null && <p role="alert">{error}</p>}
        {hasMore && !loading && (
          <button type="button" className={styles.loadOlderButton} onClick={() => void loadOlder()}>
            Load older messages
          </button>
        )}
        <DirectMessageList messages={messages} currentLogin={currentLogin} />
      </div>
      {typingUser !== null && <p className={styles.statusText}>{typingUser} is typing…</p>}
      <DirectMessageComposer onSend={sendMessage} onTyping={sendTyping} />
    </section>
  );
}
