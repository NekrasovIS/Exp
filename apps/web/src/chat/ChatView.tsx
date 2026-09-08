// Issue #267 — top-level channel content area. Encrypted channels
// (issue #136/#138) aren't decryptable here yet (that's #269) — rather
// than open a live subscription and render ciphertext, this shows a
// placeholder and never mounts ChatViewContent (and so never calls
// useMessages/useChatSocket) for one at all.

import { ChatViewContent } from "./ChatViewContent.js";

interface ChatViewProps {
  channelId: number;
  communityId: number;
  isEncrypted: boolean;
}

export function ChatView({ channelId, communityId, isEncrypted }: ChatViewProps) {
  if (isEncrypted) {
    return <p>Encrypted channels aren't supported in the web client yet.</p>;
  }
  return <ChatViewContent channelId={channelId} communityId={communityId} />;
}
