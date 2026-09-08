// Issue #267 — composes the message list, composer, and search toggle
// for one (non-encrypted) channel. Split out of ChatView.tsx so that
// component's encrypted-channel early return never mounts this (and so
// never calls useMessages/useChatSocket) for one.

import { useState } from "react";

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
  const { messages, editedIds, loading, error, hasMore, loadOlder, sendMessage, editMessage, deleteMessage } =
    useMessages(channelId);
  const [searchOpen, setSearchOpen] = useState(false);

  return (
    <section>
      <button type="button" onClick={() => setSearchOpen((open) => !open)}>
        {searchOpen ? "Close search" : "Search"}
      </button>
      {searchOpen && <MessageSearch channelId={channelId} />}

      {loading && <p>Loading messages…</p>}
      {error !== null && <p role="alert">{error}</p>}
      {hasMore && !loading && (
        <button type="button" onClick={() => void loadOlder()}>
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
      <MessageComposer channelId={channelId} onSend={sendMessage} />
    </section>
  );
}
