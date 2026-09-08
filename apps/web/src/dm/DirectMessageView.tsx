// Issue #268 — composes the DM history list, load-older, and composer
// for one thread, mirroring ChatViewContent's shape (#267) but for the
// simpler DM feature set.

import { DirectMessageComposer } from "./DirectMessageComposer.js";
import { DirectMessageList } from "./DirectMessageList.js";
import { useDirectMessages } from "./useDirectMessages.js";

interface DirectMessageViewProps {
  threadId: number;
  otherLogin: string;
}

export function DirectMessageView({ threadId, otherLogin }: DirectMessageViewProps) {
  const { messages, loading, error, hasMore, loadOlder, sendMessage } = useDirectMessages(threadId);

  return (
    <section>
      <h2>{otherLogin}</h2>
      {loading && <p>Loading messages…</p>}
      {error !== null && <p role="alert">{error}</p>}
      {hasMore && !loading && (
        <button type="button" onClick={() => void loadOlder()}>
          Load older messages
        </button>
      )}
      <DirectMessageList messages={messages} />
      <DirectMessageComposer onSend={sendMessage} />
    </section>
  );
}
