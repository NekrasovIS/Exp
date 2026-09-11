// Issue #269 — encryption-aware channel content, mirroring
// ChatViewContent.tsx but decrypting every message body for display
// and encrypting outgoing text before it ever reaches the wire (the
// server only ever sees ciphertext). No attachments, no search — the
// same restrictions DeviceHub's own encrypted-channel UI has (issue
// #138's ChannelCrypto has no story for attachment bytes at all).

import type { ChatMessageInfo, PinnedMessageInfo } from "@devicehub/core";
import { useEffect, useState, type FormEvent } from "react";

import { CallPanel } from "../calls/CallPanel.js";
import styles from "./chatView.module.css";
import { useIsModerator } from "../communities/useIsModerator.js";
import { decryptMessage, encryptMessage } from "../crypto/channelCrypto.js";
import { useSession } from "../session/SessionContext.js";
import { MessageList } from "./MessageList.js";
import { PinnedMessagesPanel } from "./PinnedMessagesPanel.js";
import { useMessages } from "./useMessages.js";
import { usePinnedMessages } from "./usePinnedMessages.js";

interface EncryptedChatViewContentProps {
  channelId: number;
  communityId: number;
  channelKey: Uint8Array;
  // See ChatViewContent.tsx's own copy of this comment — `| undefined`
  // explicit because ChatView forwards its own possibly-undefined prop
  // verbatim, under exactOptionalPropertyTypes.
  onOnlineMembers?: ((logins: string[]) => void) | undefined;
  onPresenceChanged?: ((login: string, online: boolean) => void) | undefined;
}

const kUndecryptable = "[unable to decrypt]";

export function EncryptedChatViewContent({
  channelId,
  communityId,
  channelKey,
  onOnlineMembers,
  onPresenceChanged,
}: EncryptedChatViewContentProps) {
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
  const { pinned, pinnedIds, pin, unpin } = usePinnedMessages(channelId, socket);
  const [decrypted, setDecrypted] = useState<ReadonlyMap<number, string>>(new Map());
  const [decryptedPinned, setDecryptedPinned] = useState<PinnedMessageInfo[]>([]);
  const [body, setBody] = useState("");
  const [pinnedOpen, setPinnedOpen] = useState(false);

  // Issue #322 — same forwarding as ChatViewContent's own copy of this
  // effect (presence is never encrypted, just relayed as-is).
  useEffect(() => {
    const offOnline = onOnlineMembers !== undefined ? socket.on("onlineMembers", onOnlineMembers) : undefined;
    const offPresence =
      onPresenceChanged !== undefined ? socket.on("presenceChanged", onPresenceChanged) : undefined;
    return () => {
      offOnline?.();
      offPresence?.();
    };
  }, [socket, onOnlineMembers, onPresenceChanged]);

  useEffect(() => {
    let cancelled = false;
    void (async () => {
      const entries = await Promise.all(
        messages.map(async (message): Promise<[number, string]> => {
          const plaintext = await decryptMessage(message.body, channelKey);
          return [message.id, plaintext ?? kUndecryptable];
        }),
      );
      if (!cancelled) {
        setDecrypted(new Map(entries));
      }
    })();
    return () => {
      cancelled = true;
    };
  }, [messages, channelKey]);

  // MessageList prefills its edit draft straight from message.body, so
  // it needs the decrypted text, not the raw ciphertext this hook
  // otherwise carries.
  const decryptedMessages: ChatMessageInfo[] = messages.map((message) => ({
    ...message,
    body: decrypted.get(message.id) ?? "…",
  }));

  // Pinned messages carry the same ciphertext body as the regular
  // history (chat-service stores/relays it verbatim either way) — the
  // panel needs its own decrypt pass, separate from `decrypted` above,
  // since a pinned message may not currently be in `messages` at all
  // (e.g. pinned long before this page loaded the most recent window
  // of history).
  useEffect(() => {
    let cancelled = false;
    void (async () => {
      const entries = await Promise.all(
        pinned.map(async (message): Promise<PinnedMessageInfo> => ({
          ...message,
          body: (await decryptMessage(message.body, channelKey)) ?? kUndecryptable,
        })),
      );
      if (!cancelled) {
        setDecryptedPinned(entries);
      }
    })();
    return () => {
      cancelled = true;
    };
  }, [pinned, channelKey]);

  function handleSend(event: FormEvent): void {
    event.preventDefault();
    const toSend = body.trim();
    if (toSend === "") {
      return;
    }
    setBody("");
    void encryptMessage(toSend, channelKey).then((ciphertext) => sendMessage(ciphertext));
  }

  function handleEdit(id: number, newBody: string): void {
    void encryptMessage(newBody, channelKey).then((ciphertext) => editMessage(id, ciphertext));
  }

  return (
    <section className={styles.section}>
      {currentLogin !== null && <CallPanel chatClient={socket} localLogin={currentLogin} />}
      <div className={styles.header}>
        <span className={styles.encryptedBadge}>🔒 Encrypted</span>
        {pinned.length > 0 && (
          <button type="button" onClick={() => setPinnedOpen((open) => !open)}>
            📌 {pinned.length}
          </button>
        )}
      </div>
      {pinnedOpen && <PinnedMessagesPanel pinned={decryptedPinned} />}
      <div className={styles.scrollArea}>
        {loading && <p className={styles.statusText}>Loading messages…</p>}
        {error !== null && <p role="alert">{error}</p>}
        {hasMore && !loading && (
          <button type="button" className={styles.loadOlderButton} onClick={() => void loadOlder()}>
            Load older messages
          </button>
        )}
        <MessageList
          messages={decryptedMessages}
          editedIds={editedIds}
          pinnedIds={pinnedIds}
          currentLogin={currentLogin}
          isModerator={isModerator}
          onEdit={handleEdit}
          onDelete={deleteMessage}
          onPin={pin}
          onUnpin={unpin}
        />
      </div>
      {typingUser !== null && <p className={styles.statusText}>{typingUser} is typing…</p>}
      <form onSubmit={handleSend} className={styles.simpleComposerForm}>
        <label htmlFor="encrypted-message-body">Message</label>
        <input
          id="encrypted-message-body"
          value={body}
          onChange={(event) => {
            setBody(event.target.value);
            sendTyping();
          }}
        />
        <button type="submit">Send</button>
      </form>
    </section>
  );
}
