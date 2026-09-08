// Issue #268 — analog of DeviceHub's FriendsPanel: friends, incoming
// requests, and sending a new one. Selecting a friend opens (or
// reopens — openDmThread() is idempotent) the DM thread with them,
// handled by the caller (HomePage) via onOpenThreadWith.

import { useState, type FormEvent } from "react";

import { useFriends } from "./useFriends.js";

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
    <nav aria-label="Friends">
      {loading && <p>Loading friends…</p>}
      {error !== null && <p role="alert">{error}</p>}

      <h2>Incoming requests</h2>
      <ul>
        {incomingRequests.map((request) => (
          <li key={request.id}>
            {request.requesterLogin}
            <button type="button" onClick={() => void acceptRequest(request.id)}>
              Accept
            </button>
            <button type="button" onClick={() => void declineRequest(request.id)}>
              Decline
            </button>
          </li>
        ))}
      </ul>

      <h2>Friends</h2>
      <ul>
        {friends.map((login) => (
          <li key={login}>
            {login}
            <button type="button" onClick={() => onOpenThreadWith(login)}>
              Message
            </button>
            <button type="button" onClick={() => void removeFriend(login)}>
              Remove
            </button>
          </li>
        ))}
      </ul>

      <form onSubmit={handleSendRequest}>
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
