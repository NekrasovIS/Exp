// Issue #268 — analog of DeviceHub's FriendsPanel: friends, incoming
// requests, and sending a new one. Selecting a friend opens (or
// reopens — openDmThread() is idempotent) the DM thread with them,
// handled by the caller (HomePage) via onOpenThreadWith.

import { useState, type FormEvent } from "react";

import { useFriends } from "./useFriends.js";
import styles from "./FriendsPanel.module.css";

interface FriendsPanelProps {
  onOpenThreadWith: (login: string) => void;
}

export function FriendsPanel({ onOpenThreadWith }: FriendsPanelProps) {
  const {
    friends,
    incomingRequests,
    loading,
    error,
    sendRequest,
    acceptRequest,
    declineRequest,
    removeFriend,
  } = useFriends();
  const [recipientLogin, setRecipientLogin] = useState("");
  const [requestError, setRequestError] = useState<string | null>(null);
  const [sending, setSending] = useState(false);

  async function handleSendRequest(event: FormEvent): Promise<void> {
    event.preventDefault();
    if (recipientLogin.trim() === "") {
      return;
    }
    setRequestError(null);
    setSending(true);
    try {
      await sendRequest(recipientLogin.trim());
      setRecipientLogin("");
    } catch {
      setRequestError("Couldn't send that friend request.");
    } finally {
      setSending(false);
    }
  }

  return (
    <nav aria-label="Friends" className={styles.nav}>
      {loading && <p className={styles.mutedText}>Loading friends…</p>}
      {error !== null && <p role="alert">{error}</p>}

      <h2 className={styles.sectionTitle}>Incoming requests</h2>
      <ul className={styles.list}>
        {incomingRequests.map((request) => (
          <li key={request.id} className={styles.row}>
            <span className={styles.rowLogin}>{request.requesterLogin}</span>
            <span className={styles.rowActions}>
              <button type="button" onClick={() => void acceptRequest(request.id)}>
                Accept
              </button>
              <button type="button" onClick={() => void declineRequest(request.id)}>
                Decline
              </button>
            </span>
          </li>
        ))}
      </ul>

      <h2 className={styles.sectionTitle}>Friends</h2>
      <ul className={styles.list}>
        {friends.map((login) => (
          <li key={login} className={styles.row}>
            <span className={styles.rowLogin}>{login}</span>
            <span className={styles.rowActions}>
              <button type="button" onClick={() => onOpenThreadWith(login)}>
                Message
              </button>
              <button type="button" onClick={() => void removeFriend(login)}>
                Remove
              </button>
            </span>
          </li>
        ))}
      </ul>

      <form onSubmit={handleSendRequest} className={styles.form}>
        <label htmlFor="friend-request-login">Add a friend</label>
        <input
          id="friend-request-login"
          value={recipientLogin}
          onChange={(event) => setRecipientLogin(event.target.value)}
        />
        {requestError !== null && <p role="alert">{requestError}</p>}
        <button type="submit" disabled={sending}>
          Send request
        </button>
      </form>
    </nav>
  );
}
