// Issue #269 — encryption-aware channel content, mirroring
// ChatViewContent.tsx but decrypting every message body for display
// and encrypting outgoing text before it ever reaches the wire (the
// server only ever sees ciphertext). No attachments, no search — the
// same restrictions DeviceHub's own encrypted-channel UI has (issue
// #138's ChannelCrypto has no story for attachment bytes at all).

import type { ChatMessageInfo } from "@devicehub/core";
import { useEffect, useRef, useState, type FormEvent, type KeyboardEvent } from "react";

import { CallPanel } from "../calls/CallPanel.js";
import styles from "./chatView.module.css";
import { useIsModerator } from "../communities/useIsModerator.js";
import { decryptMessage, encryptMessage } from "../crypto/channelCrypto.js";
import { useSession } from "../session/SessionContext.js";
import { MentionSuggestions } from "./MentionSuggestions.js";
import { MessageList } from "./MessageList.js";
import { useMentionAutocomplete } from "./useMentionAutocomplete.js";
import { useMessages } from "./useMessages.js";

interface EncryptedChatViewContentProps {
  channelId: number;
  communityId: number;
  channelKey: Uint8Array;
}

const kUndecryptable = "[unable to decrypt]";

export function EncryptedChatViewContent({
  channelId,
  communityId,
  channelKey,
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
  } = useMessages(channelId);
  const [decrypted, setDecrypted] = useState<ReadonlyMap<number, string>>(new Map());
  const [body, setBody] = useState("");
  // Issue #326 — mentions are typed/matched against the plaintext body
  // before encryptMessage() ever runs on it in handleSend() below.
  const mention = useMentionAutocomplete(communityId);
  const bodyInputRef = useRef<HTMLInputElement>(null);

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

  function handleBodyChange(event: React.ChangeEvent<HTMLInputElement>): void {
    setBody(event.target.value);
    mention.handleTextChange(event.target.value, event.target.selectionStart ?? event.target.value.length);
  }

  function selectMention(login: string): void {
    const cursorPos = bodyInputRef.current?.selectionStart ?? body.length;
    const result = mention.applySuggestion(body, cursorPos, login);
    setBody(result.text);
    requestAnimationFrame(() => bodyInputRef.current?.setSelectionRange(result.cursorPos, result.cursorPos));
  }

  function handleBodyKeyDown(event: KeyboardEvent<HTMLInputElement>): void {
    if (mention.suggestions.length === 0) {
      return;
    }
    if (event.key === "ArrowDown") {
      event.preventDefault();
      mention.moveActive(1);
    } else if (event.key === "ArrowUp") {
      event.preventDefault();
      mention.moveActive(-1);
    } else if (event.key === "Enter" || event.key === "Tab") {
      event.preventDefault();
      const cursorPos = bodyInputRef.current?.selectionStart ?? body.length;
      const result = mention.applyActive(body, cursorPos);
      setBody(result.text);
      requestAnimationFrame(() =>
        bodyInputRef.current?.setSelectionRange(result.cursorPos, result.cursorPos),
      );
    } else if (event.key === "Escape") {
      mention.dismiss();
    }
  }

  return (
    <section className={styles.section}>
      {currentLogin !== null && <CallPanel chatClient={socket} localLogin={currentLogin} />}
      <div className={styles.header}>
        <span className={styles.encryptedBadge}>🔒 Encrypted</span>
      </div>
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
          currentLogin={currentLogin}
          isModerator={isModerator}
          onEdit={handleEdit}
          onDelete={deleteMessage}
        />
      </div>
      <form onSubmit={handleSend} className={styles.simpleComposerForm}>
        <label htmlFor="encrypted-message-body">Message</label>
        <input
          id="encrypted-message-body"
          ref={bodyInputRef}
          value={body}
          onChange={handleBodyChange}
          onKeyDown={handleBodyKeyDown}
        />
        <MentionSuggestions
          suggestions={mention.suggestions}
          activeIndex={mention.activeIndex}
          onSelect={selectMention}
        />
        <button type="submit">Send</button>
      </form>
    </section>
  );
}
